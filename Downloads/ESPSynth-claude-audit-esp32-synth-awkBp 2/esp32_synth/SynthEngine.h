#ifndef SYNTH_ENGINE_H
#define SYNTH_ENGINE_H

#include "Config.h"
#include "Voice.h"
#include "LFO.h"
#include "Effects.h"
#include "Synthesis.h"
#include "ModMatrix.h"
#include "Arpeggiator.h"
#include "Compressor.h"
#include "SmoothedValue.h"
#include "Reverb.h"
#include "Granular.h"
#include "Euclidean.h"
#include "Resonator.h"
#include "CombFilter.h"

constexpr uint8_t NUM_VOICES = 4;

class AudioProfiler {
public:
    void startSample() {
        startTime_ = micros();
    }

    void endSample() {
        uint32_t elapsed = micros() - startTime_;
        totalTime_ += elapsed;
        sampleCount_++;
        if (elapsed > maxTime_) maxTime_ = elapsed;
        if (elapsed < minTime_) minTime_ = elapsed;

        // Update rolling average
        float blockDurationUs = (DMA_BUFFER_SAMPLES * 1000000.0f) / SAMPLE_RATE;
        lastCpuLoad_ = (elapsed / blockDurationUs) * 100.0f;
    }

    float getCPUPercent() const { return lastCpuLoad_; }
    bool isOverloaded() const { return lastCpuLoad_ > 85.0f; }

    void printStats() {
        if (sampleCount_ == 0) {
            Serial.println("Profiler: No data collected");
            return;
        }
        uint32_t avgTime = totalTime_ / sampleCount_;
        float blockDurationUs = (DMA_BUFFER_SAMPLES * 1000000.0f) / SAMPLE_RATE;
        float cpuPercent = (avgTime / blockDurationUs) * 100.0f;

        Serial.printf("Audio CPU: %.1f%% (avg: %luus, max: %luus, min: %luus) over %lu blocks\n",
                      cpuPercent, avgTime, maxTime_, minTime_, sampleCount_);

        if (maxTime_ > blockDurationUs) {
            Serial.printf("WARNING: Task Overflow! Max time %luus exceeds block duration %.0fus\n",
                          maxTime_, blockDurationUs);
        }
        reset();
    }

    void reset() {
        totalTime_ = 0;
        sampleCount_ = 0;
        maxTime_ = 0;
        minTime_ = 0xFFFFFFFF;
    }

private:
    uint32_t startTime_;
    uint32_t totalTime_ = 0;
    uint32_t sampleCount_ = 0;
    uint32_t maxTime_ = 0;
    uint32_t minTime_ = 0xFFFFFFFF;
    float lastCpuLoad_ = 0.0f;
};

class SynthEngine {
public:
    SynthEngine();
    
    bool init();
    void start();
    void stop();
    bool isRunning() const { return running_; }
    
    // Voice management
    void noteOn(uint8_t note, uint8_t velocity);
    void noteOff(uint8_t note);
    void allNotesOff();
    
    // Pitch bend (-8192 to +8191)
    void setPitchBend(int16_t value);
    void setPitchBendRange(uint8_t semitones);
    
    // Global oscillator params (applied to all voices)
    void setOscWaveform(int osc, Waveform wf);
    void setOscDetune(int osc, float cents);
    void setOscMix(float mix);
    void setPulseWidth(int osc, float pw);
    
    Waveform getOscWaveform(int osc) const { return oscWaveforms_[osc]; }
    float getOscDetune(int osc) const { return oscDetune_[osc]; }
    float getOscMix() const { return oscMix_; }

    // Synthesis mode
    void setSynthMode(VoiceSynthMode mode);
    void setFMAmount(float amount);
    VoiceSynthMode getSynthMode() const { return synthMode_; }
    float getFMAmount() const { return fmAmount_; }
    
    // Global filter
    void setFilterCutoff(float hz);
    void setFilterResonance(float r);
    void setFilterMode(FilterMode mode);
    void setFilterEnvAmount(float amount);
    void setFilterEnvVelocity(float amount);
    void setFilterKeyTracking(float amount);
    void setFilterType(VoiceFilterType type);
    
    float getFilterCutoff() const { return filterCutoff_; }
    float getFilterResonance() const { return filterReso_; }
    FilterMode getFilterMode() const { return filterMode_; }
    VoiceFilterType getFilterType() const { return filterType_; }
    float getFilterEnvAmount() const { return filterEnvAmount_; }
    float getFilterEnvVelocity() const { return filterEnvVelocity_; }
    float getFilterKeyTracking() const { return filterKeyTracking_; }
    
    // Envelopes
    void setAmpADSR(float a, float d, float s, float r);
    void setFilterADSR(float a, float d, float s, float r);
    
    // Glide
    void setGlideTime(float ms);
    
    // LFOs
    LFO& getLFO(int index);
    static constexpr int NUM_LFOS = 2;
    
    // Mod matrix
    ModMatrix& getModMatrix() { return modMatrix_; }
    
    // Arpeggiator
    Arpeggiator& getArp() { return arp_; }
    
    // Global Resonator (single instance for master chain)
    ResonatorBank& getResonator() { return resonator_; }
    void setResonatorEnabled(bool en) { resonatorEnabled_ = en; }
    bool isResonatorEnabled() const { return resonatorEnabled_; }
    
    // Global Comb (single instance for master chain)
    CombFilter& getComb() { return comb_; }
    void setCombEnabled(bool en) { combEnabled_ = en; }
    bool isCombEnabled() const { return combEnabled_; }
    
    // Granular exciter
    GranularExciter& getGranular() { return granular_; }
    void setGranularEnabled(bool en) { granularEnabled_ = en; }
    bool isGranularEnabled() const { return granularEnabled_; }
    void setGranularMix(float mix);
    
    // Effects
    EffectsChain& getEffects() { return effects_; }
    Compressor& getCompressor() { return compressor_; }
    FDNReverb& getReverb() { return reverb_; }
    
    // Master
    void setMasterVolume(float vol);
    float getMasterVolume() const { return masterVolume_.getTarget(); }
    void setGlobalPan(float pan) { globalPan_ = pan; }
    float getGlobalPan() const { return globalPan_; }
    
    // Profiler
    void printCPUStats() {
        profiler_.printStats();
        Serial.printf("I2S underruns: %lu\n", i2sUnderrunCount_);
    }
    float getCPUPercent() { return profiler_.getCPUPercent(); }
    uint32_t getBlockCount() const { return blockCounter_; }
    uint32_t getI2SUnderrunCount() const { return i2sUnderrunCount_; }

    // Voice info
    uint8_t getActiveVoiceCount() const;
    Voice& getVoice(int i) { return voices_[i]; }

    static float midiToFreq(uint8_t note) {
        return 440.0f * powf(2.0f, (note - 69) / 12.0f);
    }

#ifdef HOST_BUILD
    void renderBlock() { processBlock(); }
    const int16_t* getBlockBuffer() const { return blockBuffer_; }
#endif

private:
    void processBlock();
    int allocateVoice(uint8_t note);
    int findVoiceForNote(uint8_t note);
    static void audioTaskWrapper(void* param);
    
    // Voices
    Voice voices_[NUM_VOICES];
    
    // Global params (copied to voices)
    Waveform oscWaveforms_[2];
    float oscDetune_[2];
    float oscMix_;
    float pulseWidth_[2];
    VoiceSynthMode synthMode_;
    float fmAmount_;
    
    float filterCutoff_;
    float filterReso_;
    FilterMode filterMode_;
    VoiceFilterType filterType_;
    float filterEnvAmount_;
    float filterEnvVelocity_;
    float filterKeyTracking_;
    
    float ampA_, ampD_, ampS_, ampR_;
    float fltA_, fltD_, fltS_, fltR_;
    float glideTime_;
    
    // Pitch bend
    SmoothedValue pitchBend_;
    uint8_t pitchBendRange_;
    
    // LFOs
    LFO lfos_[NUM_LFOS];
    
    // Mod matrix
    ModMatrix modMatrix_;
    
    // Arpeggiator
    Arpeggiator arp_;
    bool lastArpGate_;
    
    // Global Resonator and Comb (single instances, post-mix)
    ResonatorBank resonator_;
    CombFilter comb_;
    bool resonatorEnabled_;
    bool combEnabled_;
    
    // Granular exciter
    GranularExciter granular_;
    float granularMix_;
    bool granularEnabled_;
    
    // Effects
    EffectsChain effects_;
    Compressor compressor_;
    FDNReverb reverb_;
    
    // Master
    SmoothedValue masterVolume_;
    float globalPan_;
    
    // Mutex protecting Arpeggiator note state (noteOn/noteOff/allNotesOff)
    // against the audio task's per-sample arp_.process() reader.
    // Both tasks are pinned to APP_CPU; the DMA-completion ISR can preempt
    // the loop task mid-call (e.g. mid-sortNotes), so a critical section is
    // required on the write side.
    // NOTE: if ISR-side note injection is ever added (clock-pin IRQ, footswitch
    // interrupt), the audio-side read path in processBlock() will also need
    // this mux — do not add it there until that time.
    portMUX_TYPE arpMux_ = portMUX_INITIALIZER_UNLOCKED;

    // Runtime
    float currentVelocity_;
    volatile bool running_;
    volatile uint32_t blockCounter_ = 0;
    uint32_t i2sUnderrunCount_ = 0;
    float voicePanL_[NUM_VOICES];
    float voicePanR_[NUM_VOICES];
    TaskHandle_t audioTaskHandle_;
    AudioProfiler profiler_;
    int16_t blockBuffer_[DMA_BUFFER_SAMPLES * 2];
};

#endif
