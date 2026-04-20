#include "Oscillator.h"
#include <math.h>

// PolyBLEP residual for a unit rising step at t=0. t and dt are normalized (0..1).
static inline float polyBlep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    } else if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// Pre-calculated supersaw multipliers (avoid powf per sample)
static const float SUPERSAW_MULT[7] = {
    0.9937f,  // -0.11 semitones
    0.9965f,  // -0.06 semitones  
    0.9988f,  // -0.02 semitones
    1.0000f,  //  0.00 semitones
    1.0012f,  // +0.02 semitones
    1.0035f,  // +0.06 semitones
    1.0064f   // +0.11 semitones
};

Oscillator::Oscillator() : 
    phase_(0), 
    effectiveIncrement_(0),
    basePhaseIncrement_(0),
    frequency_(440.0f),
    baseFrequency_(440.0f),
    waveform_(Waveform::SAW),
    tableIndex_(2),
    amplitude_(0.5f),
    detuneCents_(0.0f),
    detuneMultiplier_(1.0f),
    pulseWidth_(0.5f),
    fmMod_(0.0f),
    pitchMod_(0.0f),
    pitchMult_(1.0f),
    noiseState_(22222)
{
    for (int i = 0; i < 7; i++) {
        supersawPhases_[i] = (uint32_t)(rand());  // Random start phases
    }
    setFrequency(440.0f);
}

void Oscillator::setFrequency(float freq, bool force) {
    if (!force && fabsf(freq - baseFrequency_) < 0.001f) return;

    baseFrequency_ = constrain(freq, 20.0f, 20000.0f);
    frequency_ = baseFrequency_ * detuneMultiplier_;
    updatePhaseIncrement();
    tableIndex_ = Wavetables::tableIndexForFreq(frequency_);
}

void Oscillator::setWaveform(Waveform wf) {
    waveform_ = wf;
}

void Oscillator::setPitchMod(float semitones) {
    if (semitones != pitchMod_) {
        pitchMod_ = semitones;
        pitchMult_ = fastExp2(pitchMod_ / 12.0f);
        updateEffectiveIncrements();
    }
}

void Oscillator::setAmplitude(float amp) {
    amplitude_ = constrain(amp, 0.0f, 1.0f);
}

void Oscillator::setDetune(float cents) {
    detuneCents_ = constrain(cents, -100.0f, 100.0f);
    detuneMultiplier_ = powf(2.0f, detuneCents_ / 1200.0f);
    frequency_ = baseFrequency_ * detuneMultiplier_;
    updatePhaseIncrement();
}

void Oscillator::setPulseWidth(float pw) {
    pulseWidth_ = constrain(pw, 0.05f, 0.95f);
}

void Oscillator::updatePhaseIncrement() {
    basePhaseIncrement_ = (uint32_t)(frequency_ * PHASE_INCREMENT_MULTIPLIER);
    updateEffectiveIncrements();
}

void Oscillator::updateEffectiveIncrements() {
    effectiveIncrement_ = (uint32_t)(basePhaseIncrement_ * pitchMult_);
}

void Oscillator::resetPhase() {
    phase_ = 0;
}

float Oscillator::getPhase() const {
    return phase_ * PHASE_TO_FLOAT;
}

void Oscillator::setPhase(float phase) {
    phase_ = (uint32_t)(phase * (float)PHASE_MAX);
}

void Oscillator::sync() {
    phase_ = 0;
}

float Oscillator::generateSupersaw(uint32_t baseIncrement) {
    if (baseIncrement == 0) return 0.0f;
    if (tableIndex_ < 0 || tableIndex_ >= NUM_OCTAVE_TABLES) {
        tableIndex_ = 0;
    }

    float sum = 0.0f;
    for (int i = 0; i < 7; i++) {
        supersawPhases_[i] += (uint32_t)(baseIncrement * SUPERSAW_MULT[i]);
        sum += Wavetables::readSaw(supersawPhases_[i], tableIndex_);
    }
    return sum * 0.143f;  // 1/7
}

float Oscillator::generateNoise() {
    // Simple xorshift PRNG for white noise
    noiseState_ ^= noiseState_ << 13;
    noiseState_ ^= noiseState_ >> 17;
    noiseState_ ^= noiseState_ << 5;
    return (float)(int32_t)noiseState_ / (float)INT32_MAX;
}

float Oscillator::process() {
    float sample = 0.0f;
    
    switch (waveform_) {
        case Waveform::SINE:
            sample = Wavetables::readSine(phase_);
            break;
            
        case Waveform::SAW:
            sample = Wavetables::readSaw(phase_, tableIndex_);
            break;
            
        case Waveform::SQUARE:
            sample = Wavetables::readSquare(phase_, tableIndex_);
            break;
            
        case Waveform::TRIANGLE:
            sample = Wavetables::readTriangle(phase_, tableIndex_);
            break;
            
        case Waveform::PULSE: {
            // For high frequencies, switch to pre-computed wavetable to avoid
            // the aliasing that occurs when dt becomes too large for BLEP to correct.
            // At frequencies approaching Nyquist (e.g., Note 108 = 4186 Hz),
            // we have only ~11 samples per cycle, making correction ineffective.
            if (frequency_ > 1500.0f) {
                // Use the square table for high frequencies
                sample = Wavetables::readSquare(phase_, tableIndex_);
            } else {
                float t = phase_ * PHASE_TO_FLOAT;
                float dt = effectiveIncrement_ * PHASE_TO_FLOAT;
                sample = (t < pulseWidth_) ? 1.0f : -1.0f;
                // PolyBLEP at rising edge (t=0, step +2) and falling edge (t=pulseWidth_, step -2)
                sample += 2.0f * polyBlep(t, dt);
                float tFall = t - pulseWidth_;
                if (tFall < 0.0f) tFall += 1.0f;
                sample -= 2.0f * polyBlep(tFall, dt);
            }
            break;
        }

        case Waveform::SUPERSAW:
            sample = generateSupersaw(effectiveIncrement_);
            break;

        case Waveform::NOISE:
            sample = generateNoise();
            break;

        default:
            sample = 0.0f;
    }

    phase_ += effectiveIncrement_;
    return sample * amplitude_;
}

float Oscillator::processWithFM(float fmInput, float fmAmount) {
    // FM synthesis: modulate phase increment
    // Use float math first to avoid early overflow, then clamp
    float fmOffset = fmInput * fmAmount * (float)basePhaseIncrement_;
    float totalIncrement = (float)basePhaseIncrement_ + fmOffset;

    // Clamp to non-negative and reasonable max (2x Nyquist)
    if (totalIncrement < 0.0f) totalIncrement = 0.0f;
    if (totalIncrement > (float)PHASE_MAX * 0.5f) totalIncrement = (float)PHASE_MAX * 0.5f;

    // Apply pitch mod on top using pre-calculated multiplier
    uint32_t effectiveIncrement = (uint32_t)(totalIncrement * pitchMult_);
    
    float sample = 0.0f;
    
    switch (waveform_) {
        case Waveform::SINE:
            sample = Wavetables::readSine(phase_);
            break;
        case Waveform::SAW:
            sample = Wavetables::readSaw(phase_, tableIndex_);
            break;
        case Waveform::SQUARE:
            sample = Wavetables::readSquare(phase_, tableIndex_);
            break;
        case Waveform::TRIANGLE:
            sample = Wavetables::readTriangle(phase_, tableIndex_);
            break;
        case Waveform::PULSE: {
            float t = phase_ * PHASE_TO_FLOAT;
            float dt = effectiveIncrement * PHASE_TO_FLOAT;
            sample = (t < pulseWidth_) ? 1.0f : -1.0f;
            // PolyBLEP at rising edge (t=0, step +2) and falling edge (t=pulseWidth_, step -2)
            sample += 2.0f * polyBlep(t, dt);
            float tFall = t - pulseWidth_;
            if (tFall < 0.0f) tFall += 1.0f;
            sample -= 2.0f * polyBlep(tFall, dt);
            break;
        }
        case Waveform::SUPERSAW:
            sample = generateSupersaw(effectiveIncrement);
            break;
        case Waveform::NOISE:
            sample = generateNoise();
            break;
        default:
            sample = Wavetables::readSine(phase_);
    }
    
    phase_ += effectiveIncrement;
    return sample * amplitude_;
}
