#ifndef HARDWARE_ENCODER_H
#define HARDWARE_ENCODER_H

#include <Arduino.h>

class HardwareEncoder {
public:
    HardwareEncoder(uint8_t pinA, uint8_t pinB, uint8_t pinSW);

    void init();
    void update(); // Call in loop or use interrupts

    int getDelta();
    int getDeltaAccelerated(); // Returns delta with acceleration
    bool isPressed();
    bool wasClicked();

private:
    uint8_t pinA_, pinB_, pinSW_;

    // Encoder state
    uint8_t state_;
    int delta_;

    // Switch state
    bool lastSwState_;
    bool clicked_;
    uint32_t lastDebounceTime_;

    // Acceleration tracking
    uint32_t lastUpdateTime_;
    static const uint32_t ACCEL_THRESHOLD_MS = 100;

    static const int8_t KNOB_STATES[];
};

#endif
