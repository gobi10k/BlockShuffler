#include "AnalogControls.h"
#include "SynthEngine.h"
#include <Arduino.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static adc_oneshot_unit_handle_t adc_handle = NULL;

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

// Map GPIO to ADC1 Channel
static adc_channel_t getChannelFromPin(int pin) {
    switch(pin) {
        case 32: return ADC_CHANNEL_4;
        case 33: return ADC_CHANNEL_5;
        case 34: return ADC_CHANNEL_6;
        case 35: return ADC_CHANNEL_7;
        case 36: return ADC_CHANNEL_0;
        case 39: return ADC_CHANNEL_3;
        default: return ADC_CHANNEL_0; // Fallback
    }
}

void AnalogControls::init(SynthEngine* engine) {
    engine_ = engine;

    if (!adc_handle) {
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
            .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        esp_err_t err = adc_oneshot_new_unit(&init_config, &adc_handle);
        if (err != ESP_OK) {
            Serial.printf("ADC Init Failed: %d\n", err);
            return;
        }

        adc_oneshot_chan_cfg_t config = {
            .atten = ADC_ATTEN_DB_11,
            .bitwidth = ADC_BITWIDTH_12,
        };

        adc_oneshot_config_channel(adc_handle, getChannelFromPin(POT_CUTOFF_PIN), &config);
        adc_oneshot_config_channel(adc_handle, getChannelFromPin(POT_RESO_PIN), &config);
        adc_oneshot_config_channel(adc_handle, getChannelFromPin(POT_VOLUME_PIN), &config);
        adc_oneshot_config_channel(adc_handle, getChannelFromPin(POT_EFFECT_PIN), &config);
    }
}

void AnalogControls::readPot(int index, int pin) {
    if (!adc_handle) return;
    
    int raw = 0;
    esp_err_t err = adc_oneshot_read(adc_handle, getChannelFromPin(pin), &raw);
    if (err != ESP_OK) return;

    // Smooth using moving average
    potHistory_[index][historyIndex_[index]] = raw;
    historyIndex_[index] = (historyIndex_[index] + 1) % HISTORY_SIZE;

    int sum = 0;
    for (int i = 0; i < HISTORY_SIZE; i++) {
        sum += potHistory_[index][i];
    }
    currentValues_[index] = sum / HISTORY_SIZE;
}

void AnalogControls::update() {
    if (!engine_) return;

    readPot(0, POT_CUTOFF_PIN);
    readPot(1, POT_RESO_PIN);
    readPot(2, POT_VOLUME_PIN);
    readPot(3, POT_EFFECT_PIN);

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
