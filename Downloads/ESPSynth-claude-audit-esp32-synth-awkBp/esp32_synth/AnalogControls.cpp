#include "AnalogControls.h"
#include "SynthEngine.h"
#include <Arduino.h>

adc_oneshot_unit_handle_t AnalogControls::adc1_handle_ = nullptr;
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

    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    esp_err_t err = adc_oneshot_new_unit(&init_config1, &adc1_handle_);
    if (err != ESP_OK) {
        Serial.printf("ERROR: ADC1 init failed: %s\n", esp_err_to_name(err));
        return;
    }

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_12,
    };

    adc_oneshot_config_channel(adc1_handle_, ADC_CHANNEL_4, &config);
    adc_oneshot_config_channel(adc1_handle_, ADC_CHANNEL_5, &config);
    adc_oneshot_config_channel(adc1_handle_, ADC_CHANNEL_6, &config);
    adc_oneshot_config_channel(adc1_handle_, ADC_CHANNEL_7, &config);

    adc_initialized_ = true;
    Serial.println("ADC1 initialized via adc_oneshot driver");
}

void AnalogControls::init(SynthEngine* engine) {
    engine_ = engine;
    initADC();
}

int AnalogControls::readPot(int index) {
    if (!adc_initialized_) {
        return 0;
    }

    adc_channel_t channels[NUM_POTS] = {
        ADC_CHANNEL_4,  // POT_CUTOFF (GPIO32)
        ADC_CHANNEL_5,  // POT_RESO (GPIO33)
        ADC_CHANNEL_6,  // POT_VOLUME (GPIO34)
        ADC_CHANNEL_7   // POT_EFFECT (GPIO35)
    };

    int raw = 0;
    esp_err_t err = adc_oneshot_read(adc1_handle_, channels[index], &raw);
    if (err != ESP_OK) {
        Serial.printf("ADC read error: %s\n", esp_err_to_name(err));
        return 0;
    }

    // Smooth using moving average
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