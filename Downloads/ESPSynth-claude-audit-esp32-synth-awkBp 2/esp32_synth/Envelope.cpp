#include "Envelope.h"
#include <math.h>

Envelope::Envelope() :
    attackTime_(0.01f),
    decayTime_(0.1f),
    sustainLevel_(0.7f),
    releaseTime_(0.3f),
    currentValue_(0.0f),
    targetValue_(0.0f),
    stage_(EnvelopeStage::IDLE),
    gateOn_(false)
{
    setADSR(attackTime_, decayTime_, sustainLevel_, releaseTime_);
}

float Envelope::calcCoef(float timeSeconds) const {
    if (timeSeconds <= 0.0f) return 1.0f;
    
    float samples = timeSeconds * SAMPLE_RATE;
    
    // For very short times (< 50 samples / ~1ms), use a moderate coefficient for snappy attack.
    // The coefficient formula uses exponential approach: next = current + coef * (target - current)
    // To get punchy feel with fast initial rise, use moderate coefficient.
    if (samples < 50.0f) {
        return 0.5f;  // ~50% target in one step, then 75%, then 87.5%...
    }
    
    // Standard exponential approach (reaches ~63% in time seconds)
    float coef = 1.0f - expf(-1.0f / samples);
    return constrain(coef, 0.0f, 0.9999f);
}

void Envelope::setAttack(float seconds) {
    attackTime_ = max(0.001f, seconds);
    attackCoef_ = calcCoef(attackTime_);
}

void Envelope::setDecay(float seconds) {
    decayTime_ = max(0.001f, seconds);
    decayCoef_ = calcCoef(decayTime_);
}

void Envelope::setSustain(float level) {
    sustainLevel_ = constrain(level, 0.0f, 1.0f);
}

void Envelope::setRelease(float seconds) {
    releaseTime_ = max(0.001f, seconds);
    releaseCoef_ = calcCoef(releaseTime_);
}

void Envelope::setADSR(float a, float d, float s, float r) {
    setAttack(a);
    setDecay(d);
    setSustain(s);
    setRelease(r);
}

void Envelope::gate(bool on) {
    if (on && !gateOn_) {
        // Gate on - start attack
        stage_ = EnvelopeStage::ATTACK;
        targetValue_ = 1.0f;
    } else if (!on && gateOn_) {
        // Gate off - start release
        stage_ = EnvelopeStage::RELEASE;
        targetValue_ = 0.0f;
    }
    gateOn_ = on;
}

void Envelope::trigger() {
    // Retrigger from current position
    stage_ = EnvelopeStage::ATTACK;
    targetValue_ = 1.0f;
    gateOn_ = true;
}

float Envelope::process() {
    switch (stage_) {
        case EnvelopeStage::IDLE:
            currentValue_ = 0.0f;
            break;
            
        case EnvelopeStage::ATTACK:
            currentValue_ += attackCoef_ * (targetValue_ - currentValue_);
            if (currentValue_ >= 0.99f) {
                currentValue_ = 1.0f;
                stage_ = EnvelopeStage::DECAY;
                targetValue_ = sustainLevel_;
            }
            break;
            
        case EnvelopeStage::DECAY:
            currentValue_ += decayCoef_ * (targetValue_ - currentValue_);
            if (currentValue_ <= sustainLevel_ + 0.001f) {
                currentValue_ = sustainLevel_;
                stage_ = EnvelopeStage::SUSTAIN;
            }
            break;
            
        case EnvelopeStage::SUSTAIN:
            currentValue_ = sustainLevel_;
            break;
            
        case EnvelopeStage::RELEASE:
            currentValue_ += releaseCoef_ * (targetValue_ - currentValue_);
            if (currentValue_ <= 0.001f) {
                currentValue_ = 0.0f;
                stage_ = EnvelopeStage::IDLE;
            }
            break;
    }
    
    return currentValue_;
}
