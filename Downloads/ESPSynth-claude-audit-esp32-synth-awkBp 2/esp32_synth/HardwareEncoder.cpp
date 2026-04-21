#include "HardwareEncoder.h"

// 4-state encoder logic
const int8_t HardwareEncoder::KNOB_STATES[] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0
};

HardwareEncoder::HardwareEncoder(uint8_t pinA, uint8_t pinB, uint8_t pinSW) :
    pinA_(pinA), pinB_(pinB), pinSW_(pinSW),
    state_(0), delta_(0),
    lastSwState_(true), clicked_(false), lastDebounceTime_(0),
    lastUpdateTime_(0)
{
}

void HardwareEncoder::init() {
    pinMode(pinA_, INPUT_PULLUP);
    pinMode(pinB_, INPUT_PULLUP);
    pinMode(pinSW_, INPUT_PULLUP);

    // Initial state
    uint8_t a = digitalRead(pinA_);
    uint8_t b = digitalRead(pinB_);
    state_ = (a << 1) | b;
}

void HardwareEncoder::update() {
    // 1. Encoder reading
    uint8_t a = digitalRead(pinA_);
    uint8_t b = digitalRead(pinB_);
    uint8_t currentState = (a << 1) | b;

    if (currentState != state_) {
        // Index by [oldState][newState]
        // oldState is state_, newState is currentState
        // index = state_ * 4 + currentState
        delta_ += KNOB_STATES[state_ * 4 + currentState];
        state_ = currentState;
    }

    // 2. Switch reading (de-bounced)
    bool sw = digitalRead(pinSW_);
    if (sw != lastSwState_) {
        if (millis() - lastDebounceTime_ > 50) {
            if (lastSwState_ && !sw) {
                // Falling edge (pressed)
                clicked_ = true;
            }
            lastSwState_ = sw;
            lastDebounceTime_ = millis();
        }
    }
}

int HardwareEncoder::getDelta() {
    int d = delta_;
    delta_ = 0;
    if (abs(d) >= 2) {
        int result = d / 2;
        delta_ = d % 2;
        return result;
    }
    return 0;
}

int HardwareEncoder::getDeltaAccelerated() {
    int d = getDelta();
    if (d == 0) return 0;

    uint32_t now = millis();
    uint32_t elapsed = now - lastUpdateTime_;
    lastUpdateTime_ = now;

    if (elapsed < ACCEL_THRESHOLD_MS) {
        // Accelerate: double after first fast turn, triple after second
        int acceleration = (elapsed < 50) ? 3 : 2;
        return d * acceleration;
    }
    return d;
}

bool HardwareEncoder::isPressed() {
    return !digitalRead(pinSW_);
}

bool HardwareEncoder::wasClicked() {
    bool c = clicked_;
    clicked_ = false;
    return c;
}
