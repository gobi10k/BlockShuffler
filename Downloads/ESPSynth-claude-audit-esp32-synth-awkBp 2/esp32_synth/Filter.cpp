#include "Filter.h"
#include <math.h>

const char* FILTER_MODE_NAMES[] = {"LPF", "HPF", "BPF", "NOTCH"};

Filter::Filter() :
    cutoffHz_(1000.0f),
    resonance_(0.0f),
    mode_(FilterMode::LOWPASS),
    keyTracking_(0.0f),
    keyFreq_(440.0f),
    cutoffMod_(0.0f),
    fMod_(0.0f),
    q_(1.0f),
    low_(0.0f),
    high_(0.0f),
    band_(0.0f),
    notch_(0.0f)
{
    updateCoefficients(0.0f);
}

void Filter::setCutoff(float hz) {
    cutoffHz_ = constrain(hz, 20.0f, 20000.0f);
    updateCoefficients(0.0f);
}

void Filter::setResonance(float r) {
    resonance_ = constrain(r, 0.0f, 1.0f);
    // Q from ~0.707 (no resonance, Butterworth) to ~0.0 (self-oscillation)
    // At resonance=0.99, q_ approaches 0 for self-oscillation
    q_ = 0.707f * (1.0f - resonance_);
}

void Filter::setMode(FilterMode mode) {
    mode_ = mode;
}

void Filter::setKeyTracking(float amount) {
    keyTracking_ = constrain(amount, 0.0f, 1.0f);
}

void Filter::updateCoefficients(float modHz) {
    float keyOffset = 0.0f;
    if (keyTracking_ > 0.0f) {
        keyOffset = (keyFreq_ - 440.0f) * keyTracking_ * 1.5f;
    }

    float modFreq = cutoffHz_ + modHz + keyOffset;
    modFreq = constrain(modFreq, 20.0f, 20000.0f);
    float normalizedFreq = modFreq / SAMPLE_RATE;
    
    // Self-oscillation: at resonance >= 0.99, ensure q_ is exactly 0
    // to allow the filter to oscillate freely
    if (resonance_ >= 0.99f) {
        q_ = 0.0f;
    }
    
    fMod_ = 2.0f * sinf(M_PI * min(normalizedFreq, 0.45f));
}

void Filter::reset() {
    low_ = 0.0f;
    high_ = 0.0f;
    band_ = 0.0f;
    notch_ = 0.0f;
}

float Filter::process(float input) {
    // Guard at entry: NaN/Inf input must not be written into integrator state.
    // Guard state: if state is already corrupt (e.g. from a prior NaN), reset
    // so the voice can recover on subsequent samples rather than staying silent.
    if (isnan(input) || isinf(input)) return 0.0f;
    if (isnan(low_) || isinf(low_) || isnan(band_) || isinf(band_)) reset();

    // State variable filter iteration (2x oversampled for stability)
    for (int i = 0; i < 2; i++) {
        low_ += fMod_ * band_;
        high_ = input - low_ - q_ * band_;
        band_ += fMod_ * high_;
        notch_ = high_ + low_;
    }
    // Flush integrators to zero to avoid Xtensa LX6 denormal penalty in long tails
    if (fabsf(low_)  < 1e-20f) low_  = 0.0f;
    if (fabsf(band_) < 1e-20f) band_ = 0.0f;

    // Select output based on mode
    switch (mode_) {
        case FilterMode::LOWPASS:
            return low_;
        case FilterMode::HIGHPASS:
            return high_;
        case FilterMode::BANDPASS:
            return band_;
        case FilterMode::NOTCH:
            return notch_;
        default:
            return low_;
    }
}
