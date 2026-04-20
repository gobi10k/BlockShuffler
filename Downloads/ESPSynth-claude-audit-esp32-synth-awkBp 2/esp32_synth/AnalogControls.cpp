#include "AnalogControls.h"
#include "SynthEngine.h"

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

void AnalogControls::init(SynthEngine* engine) {
    engine_ = engine;
    // ADC permanently disabled for Phase 1 to bypass deep ESP-IDF driver conflicts.
}

void AnalogControls::readPot(int index, int pin) {
    // Disabled
}

void AnalogControls::update() {
    // Disabled
}

