#include "AnalogControls.h"
#include "SynthEngine.h"
#include <Arduino.h>

bool AnalogControls::adc_initialized_ = false;

AnalogControls::AnalogControls() :
    engine_(nullptr)
{
    for (int i = 0; i < NUM_POTS; i++) {
        historyIndex_[i] = 0;
        lastValues_[i] = -1;
        currentValues_[i] = 0;
        for (int j = 0; j < HISTORY_SIZE; j++) {
            potHistory_[i][j] = 0;
        }
    }
}

void AnalogControls::initADC() {
    if (adc_initialized_) {
        return;
    }

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    adc_initialized_ = true;
    Serial.println("ADC initialized via legacy driver (analogRead)");
}

void AnalogControls::init(SynthEngine* engine) {
    engine_ = engine;
    initADC();
}

int AnalogControls::readPot(int index) {
    if (!adc_initialized_) {
        return 0;
    }

    uint8_t pins[NUM_POTS] = {
        POT_CUTOFF_PIN,
        POT_RESO_PIN,
        POT_VOLUME_PIN,
        POT_EFFECT_PIN
    };

    int raw = analogRead(pins[index]);

    potHistory_[index][historyIndex_[index]] = raw;
    historyIndex_[index] = (historyIndex_[index] + 1) % HISTORY_SIZE;

    int sum = 0;
    for (int i = 0; i < HISTORY_SIZE; i++) {
        sum += potHistory_[index][i];
    }
    return sum / HISTORY_SIZE;
}

void AnalogControls::update() {
    if (!engine_) return;

    currentValues_[0] = readPot(0);
    currentValues_[1] = readPot(1);
    currentValues_[2] = readPot(2);
    currentValues_[3] = readPot(3);

    // Apply changes with deadzone
    for (int i = 0; i < NUM_POTS; i++) {
        if (abs(currentValues_[i] - lastValues_[i]) > DEADZONE) {
            float normalized = currentValues_[i] / 4095.0f;

            switch (i) {
                case 0: // Cutoff: 20Hz to 12000Hz (exponential-ish)
                    engine_->setFilterCutoff(20.0f + (normalized * normalized) * 11980.0f);
                    break;
                case 1: // Resonance
                    engine_->setFilterResonance(normalized);
                    break;
                case 2: // Volume
                    engine_->setMasterVolume(normalized);
                    break;
                case 3: // Effect Mix (using Reverb as primary)
                    engine_->getReverb().setMix(normalized);
                    if (normalized > 0.05f) engine_->getReverb().setEnabled(true);
                    else engine_->getReverb().setEnabled(false);
                    break;
            }
            lastValues_[i] = currentValues_[i];
        }
    }
}