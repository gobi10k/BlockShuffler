#ifndef ANALOG_CONTROLS_H
#define ANALOG_CONTROLS_H

#include "SynthEngine.h"
#include "esp_adc/adc_oneshot.h"

class AnalogControls {
public:
    static constexpr int NUM_POTS = 4;
    static constexpr int HISTORY_SIZE = 8;
    static constexpr int DEADZONE = 10;
    
    static_assert(NUM_POTS == 4, "Update for pot count");

    AnalogControls();
    static void initADC();
    void init(SynthEngine* engine);
    void update();

private:
    int readPot(int index);

    static adc_oneshot_unit_handle_t adc1_handle_;
    static bool adc_initialized_;
    SynthEngine* engine_;
    int historyIndex_[NUM_POTS];
    int lastValues_[NUM_POTS];
    int currentValues_[NUM_POTS];
    int potHistory_[NUM_POTS][HISTORY_SIZE];
};

#endif