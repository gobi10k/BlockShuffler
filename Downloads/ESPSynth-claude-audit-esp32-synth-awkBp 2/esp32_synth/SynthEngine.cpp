#include "SynthEngine.h"
#include "Wavetables.h"
#include "MathUtils.h"
#include <driver/i2s.h>
#include <math.h>

SynthEngine::SynthEngine() :
    oscMix_(0.5f),
    filterCutoff_(2000.0f),
    filterReso_(0.3f),
    filterMode_(FilterMode::LOWPASS),
    filterType_(VoiceFilterType::SVF),
    filterEnvAmount_(0.5f),
    filterEnvVelocity_(0.5f),
    filterKeyTracking_(0.5f),
    ampA_(0.01f), ampD_(0.1f), ampS_(0.7f), ampR_(0.3f),
    fltA_(0.01f), fltD_(0.2f), fltS_(0.3f), fltR_(0.5f),
    glideTime_(0.0f),
    pitchBendRange_(2),
    lastArpGate_(false),
    synthMode_(VoiceSynthMode::STANDARD),
    fmAmount_(1.0f),
    resonatorEnabled_(true),
    combEnabled_(false),
    granularMix_(0.2f),
    granularEnabled_(false),
    globalPan_(0.0f),
    currentVelocity_(0.0f),
    running_(false),
    audioTaskHandle_(nullptr)
{
    oscWaveforms_[0] = Waveform::SAW;
    oscWaveforms_[1] = Waveform::SAW;
    oscDetune_[0] = 0.0f;
    oscDetune_[1] = 7.0f;
    pulseWidth_[0] = 0.5f;
    pulseWidth_[1] = 0.5f;
    
    pitchBend_.setImmediate(0.0f);
    pitchBend_.setSmoothTime(5.0f);
    
    masterVolume_.setImmediate(0.7f);
    masterVolume_.setSmoothTime(10.0f);
    
    // Setup LFOs
    lfos_[0].setFrequency(2.0f);
    lfos_[0].setWaveform(LFOWaveform::SINE);
    lfos_[1].setFrequency(0.2f);
    lfos_[1].setWaveform(LFOWaveform::TRIANGLE);
    
    // Default mod routing - LFO1 to filter cutoff with significant amount
    modMatrix_.setSlot(0, ModSource::LFO1, ModDest::FILTER_CUTOFF, 0.5f);

    // Resonator defaults - harmonic profile with moderate mix
    resonator_.setProfile(ResonatorProfile::HARMONIC);
    resonator_.setMix(0.4f);

    // Mod wheel to resonator mix
    modMatrix_.setSlot(1, ModSource::MOD_WHEEL, ModDest::RESONATOR_MIX, 0.5f);
}

bool SynthEngine::init() {
    Wavetables::init();
    
    i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUFFER_COUNT,
        .dma_buf_len = DMA_BUFFER_SAMPLES,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pinConfig = {
        .mck_io_num = I2S_MCK_PIN,
        .bck_io_num = I2S_BCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_DATA_OUT_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[SynthEngine] I2S install failed: %d\n", err);
        return false;
    }
    
    err = i2s_set_pin(I2S_NUM_0, &pinConfig);
    if (err != ESP_OK) {
        Serial.printf("[SynthEngine] I2S pin config failed: %d\n", err);
        return false;
    }
    
    Serial.println("[SynthEngine] Ready");
    return true;
}

void SynthEngine::start() {
    if (running_) return;
    running_ = true;
    
    xTaskCreatePinnedToCore(
        audioTaskWrapper,
        "SynthTask",
        20480,  // Further increased stack size to 20KB
        this,
        configMAX_PRIORITIES - 1,
        &audioTaskHandle_,
        1
    );
}

void SynthEngine::stop() {
    running_ = false;
    if (audioTaskHandle_) {
        vTaskDelay(pdMS_TO_TICKS(50));
        vTaskDelete(audioTaskHandle_);
        audioTaskHandle_ = nullptr;
    }
}

int SynthEngine::findVoiceForNote(uint8_t note) {
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices_[i].isActive() && voices_[i].getNote() == note) {
            return i;
        }
    }
    return -1;
}

int SynthEngine::allocateVoice(uint8_t note) {
    // First: find a free voice
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices_[i].isFree()) {
            return i;
        }
    }
    
    // Second: steal the oldest releasing voice
    int oldestReleasing = -1;
    uint32_t maxAge = 0;
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices_[i].isReleasing() && voices_[i].getAge() > maxAge) {
            maxAge = voices_[i].getAge();
            oldestReleasing = i;
        }
    }
    if (oldestReleasing >= 0) {
        voices_[oldestReleasing].forceOff();
        return oldestReleasing;
    }
    
    // Third: steal the oldest active voice
    int oldest = 0;
    maxAge = 0;
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices_[i].getAge() > maxAge) {
            maxAge = voices_[i].getAge();
            oldest = i;
        }
    }
    voices_[oldest].forceOff();
    return oldest;
}

void SynthEngine::noteOn(uint8_t note, uint8_t velocity) {
    currentVelocity_ = velocity / 127.0f;

    // If arpeggiator is on, feed it instead
    if (arp_.getMode() != ArpMode::OFF) {
        taskENTER_CRITICAL(&arpMux_);
        arp_.noteOn(note, velocity);
        taskEXIT_CRITICAL(&arpMux_);
        return;
    }
    
    int voice = allocateVoice(note);
    if (voice < 0) return; // Should not happen with stealing
    
    // Configure voice (minimal settings)
    voices_[voice].setOscWaveform(0, oscWaveforms_[0]);
    voices_[voice].setOscWaveform(1, oscWaveforms_[1]);
    voices_[voice].setOscMix(oscMix_);
    voices_[voice].setSynthMode(synthMode_);
    voices_[voice].setFMAmount(fmAmount_);
    voices_[voice].setFilterType(filterType_);
    voices_[voice].setFilterCutoff(filterCutoff_);
    voices_[voice].setFilterResonance(filterReso_);
    voices_[voice].setFilterMode(filterMode_);
    voices_[voice].setFilterEnvAmount(filterEnvAmount_);
    voices_[voice].setFilterEnvVelocity(filterEnvVelocity_);
    voices_[voice].setFilterKeyTracking(filterKeyTracking_);
    voices_[voice].setAmpADSR(ampA_, ampD_, ampS_, ampR_);
    voices_[voice].setFilterADSR(fltA_, fltD_, fltS_, fltR_);

    // Combine spread and global pan
    float spread = -0.7f + (1.4f * voice / (NUM_VOICES - 1));
    voices_[voice].setPan(constrain(spread + globalPan_, -1.0f, 1.0f));

    // Melodic tracking for resonator
    if (resonatorEnabled_) {
        resonator_.setFrequency(midiToFreq(note));
    }

    voices_[voice].noteOn(note, velocity);
}

void SynthEngine::noteOff(uint8_t note) {
    if (arp_.getMode() != ArpMode::OFF) {
        taskENTER_CRITICAL(&arpMux_);
        arp_.noteOff(note);
        taskEXIT_CRITICAL(&arpMux_);
        return;
    }
    
    // Find voice playing this note
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices_[i].isActive() && voices_[i].getNote() == note) {
            voices_[i].noteOff();
            break;
        }
    }
}

void SynthEngine::allNotesOff() {
    taskENTER_CRITICAL(&arpMux_);
    arp_.allNotesOff();
    taskEXIT_CRITICAL(&arpMux_);
    for (int i = 0; i < NUM_VOICES; i++) {
        voices_[i].forceOff();
    }
}

void SynthEngine::setPitchBend(int16_t value) {
    // Convert to semitones
    float semitones = (value / 8192.0f) * pitchBendRange_;
    pitchBend_.setTarget(semitones);
}

void SynthEngine::setPitchBendRange(uint8_t semitones) {
    pitchBendRange_ = constrain(semitones, 1, 24);
}

void SynthEngine::setOscWaveform(int osc, Waveform wf) {
    if (osc >= 0 && osc < 2) {
        oscWaveforms_[osc] = wf;
        for (int i = 0; i < NUM_VOICES; i++) {
            voices_[i].setOscWaveform(osc, wf);
        }
    }
}

void SynthEngine::setOscDetune(int osc, float cents) {
    if (osc >= 0 && osc < 2) {
        oscDetune_[osc] = cents;
        for (int i = 0; i < NUM_VOICES; i++) {
            voices_[i].setOscDetune(osc, cents);
        }
    }
}

void SynthEngine::setOscMix(float mix) {
    oscMix_ = constrain(mix, 0.0f, 1.0f);
    for (int i = 0; i < NUM_VOICES; i++) {
        voices_[i].setOscMix(mix);
    }
}

void SynthEngine::setSynthMode(VoiceSynthMode mode) {
    synthMode_ = mode;
    for (int i = 0; i < NUM_VOICES; i++) {
        voices_[i].setSynthMode(mode);
    }
}

void SynthEngine::setFMAmount(float amount) {
    fmAmount_ = amount;
    for (int i = 0; i < NUM_VOICES; i++) {
        voices_[i].setFMAmount(amount);
    }
}

void SynthEngine::setPulseWidth(int osc, float pw) {
    if (osc >= 0 && osc < 2) {
        pulseWidth_[osc] = pw;
    }
}

void SynthEngine::setFilterCutoff(float hz) {
    filterCutoff_ = constrain(hz, 20.0f, 20000.0f);
    for (int i = 0; i < NUM_VOICES; i++) {
        voices_[i].setFilterCutoff(hz);
    }
}

void SynthEngine::setFilterResonance(float r) {
    filterReso_ = constrain(r, 0.0f, 1.0f);
    for (int i = 0; i < NUM_VOICES; i++) {
        voices_[i].setFilterResonance(r);
    }
}

void SynthEngine::setFilterMode(FilterMode mode) {
    filterMode_ = mode;
    for (int i = 0; i < NUM_VOICES; i++) {
        voices_[i].setFilterMode(mode);
    }
}

void SynthEngine::setFilterEnvAmount(float amount) {
    filterEnvAmount_ = constrain(amount, -1.0f, 1.0f);
    for (int i = 0; i < NUM_VOICES; i++) {
        voices_[i].setFilterEnvAmount(amount);
    }
}

void SynthEngine::setFilterEnvVelocity(float amount) {
    filterEnvVelocity_ = constrain(amount, 0.0f, 1.0f);
    for (int i = 0; i < NUM_VOICES; i++) {
        voices_[i].setFilterEnvVelocity(amount);
    }
}

void SynthEngine::setFilterKeyTracking(float amount) {
    filterKeyTracking_ = constrain(amount, 0.0f, 1.0f);
    for (int i = 0; i < NUM_VOICES; i++) {
        voices_[i].setFilterKeyTracking(amount);
    }
}

void SynthEngine::setFilterType(VoiceFilterType type) {
    filterType_ = type;
    // Critical section: ISR can preempt mid-iteration leaving some voices on
    // the old type and some on the new — same race as arpeggiator sortNotes.
    taskENTER_CRITICAL(&arpMux_);
    for (int i = 0; i < NUM_VOICES; i++) {
        voices_[i].setFilterType(type);
    }
    taskEXIT_CRITICAL(&arpMux_);
}

void SynthEngine::setAmpADSR(float a, float d, float s, float r) {
    // -1 means don't change
    if (a >= 0) ampA_ = a;
    if (d >= 0) ampD_ = d;
    if (s >= 0) ampS_ = s;
    if (r >= 0) ampR_ = r;
    for (int i = 0; i < NUM_VOICES; i++) {
        voices_[i].setAmpADSR(ampA_, ampD_, ampS_, ampR_);
    }
}

void SynthEngine::setFilterADSR(float a, float d, float s, float r) {
    if (a >= 0) fltA_ = a;
    if (d >= 0) fltD_ = d;
    if (s >= 0) fltS_ = s;
    if (r >= 0) fltR_ = r;
    for (int i = 0; i < NUM_VOICES; i++) {
        voices_[i].setFilterADSR(fltA_, fltD_, fltS_, fltR_);
    }
}

void SynthEngine::setGlideTime(float ms) {
    glideTime_ = ms;
    for (int i = 0; i < NUM_VOICES; i++) {
        voices_[i].setGlideTime(ms);
    }
}

LFO& SynthEngine::getLFO(int index) {
    return lfos_[index % NUM_LFOS];
}

void SynthEngine::setGranularMix(float mix) {
    granularMix_ = constrain(mix, 0.0f, 1.0f);
}

void SynthEngine::setMasterVolume(float vol) {
    masterVolume_.setTarget(constrain(vol, 0.0f, 1.0f));
}

uint8_t SynthEngine::getActiveVoiceCount() const {
    uint8_t count = 0;
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices_[i].isActive()) count++;
    }
    return count;
}

void SynthEngine::processBlock() {
    profiler_.startSample();
    blockCounter_++;

    // --- Control Rate Update (Once per 128 samples) ---
    // Massive CPU optimization: Move slow modulation out of the sample loop
    float lfo1 = lfos_[0].process(DMA_BUFFER_SAMPLES);
    float lfo2 = lfos_[1].process(DMA_BUFFER_SAMPLES);
    modMatrix_.setSourceValue(ModSource::LFO1, lfo1);
    modMatrix_.setSourceValue(ModSource::LFO2, lfo2);
    modMatrix_.setSourceValue(ModSource::VELOCITY, currentVelocity_);
    modMatrix_.process();
    float filterMod = modMatrix_.getModulation(ModDest::FILTER_CUTOFF);
    float pitchMod = modMatrix_.getModulation(ModDest::OSC_PITCH) * 2.0f;

    for (int v = 0; v < NUM_VOICES; v++) {
        if (voices_[v].isActive()) {
            voices_[v].setGlobalFilterMod(filterMod);
            voices_[v].setGlobalPitchMod(pitchMod);
            voices_[v].updateBlockParams();

            // Pre-calculate panning coefficients
            float pan = voices_[v].getPan();
            float panAngle = (pan + 1.0f) * 0.785398f;
            voicePanL_[v] = cosf(panAngle);
            voicePanR_[v] = sinf(panAngle);
        }
    }
    // --------------------------------------------------
    
    for (int i = 0; i < DMA_BUFFER_SAMPLES; i++) {
        // Process arpeggiator (only if mode is not OFF and we have notes)
        if (arp_.getMode() != ArpMode::OFF) {
            if (arp_.process()) {
                if (lastArpGate_) {
                    for (int v = 0; v < NUM_VOICES; v++) {
                        voices_[v].noteOff();
                    }
                }
                
                int voice = allocateVoice(arp_.getCurrentNote());
                if (voice >= 0 && voice < NUM_VOICES) {
                    voices_[voice].setOscWaveform(0, oscWaveforms_[0]);
                    voices_[voice].setOscWaveform(1, oscWaveforms_[1]);
                    voices_[voice].setOscDetune(0, oscDetune_[0]);
                    voices_[voice].setOscDetune(1, oscDetune_[1]);
                    voices_[voice].setOscMix(oscMix_);
                    voices_[voice].setSynthMode(synthMode_);
                    voices_[voice].setFMAmount(fmAmount_);
                    voices_[voice].setFilterType(filterType_);
                    voices_[voice].setFilterCutoff(filterCutoff_);
                    voices_[voice].setFilterResonance(filterReso_);
                    voices_[voice].setFilterMode(filterMode_);
                    voices_[voice].setFilterEnvAmount(filterEnvAmount_);
                    voices_[voice].setFilterEnvVelocity(filterEnvVelocity_);
                    voices_[voice].setFilterKeyTracking(filterKeyTracking_);
                    voices_[voice].setAmpADSR(ampA_, ampD_, ampS_, ampR_);
                    voices_[voice].setFilterADSR(fltA_, fltD_, fltS_, fltR_);
                    voices_[voice].setGlideTime(glideTime_);

                    // Combine spread and global pan
                    float spread = -0.7f + (1.4f * voice / (NUM_VOICES - 1));
                    voices_[voice].setPan(constrain(spread + globalPan_, -1.0f, 1.0f));
                    // Refresh pan coefficients now; block-start precompute used stale values.
                    float arpPanAngle = (voices_[voice].getPan() + 1.0f) * 0.785398f;
                    voicePanL_[voice] = cosf(arpPanAngle);
                    voicePanR_[voice] = sinf(arpPanAngle);

                    currentVelocity_ = arp_.getCurrentVelocity() / 127.0f;

                    if (resonatorEnabled_) {
                        resonator_.setFrequency(midiToFreq(arp_.getCurrentNote()));
                    }

                    voices_[voice].noteOn(arp_.getCurrentNote(), arp_.getCurrentVelocity());
                }
            }
            
            // Handle arp gate off
            if (lastArpGate_ && !arp_.isGateOn()) {
                for (int v = 0; v < NUM_VOICES; v++) {
                    if (voices_[v].isActive()) {
                        voices_[v].noteOff();
                    }
                }
            }
            lastArpGate_ = arp_.isGateOn();
        }
        
        // Mix all voices with panning
        float left = 0.0f;
        float right = 0.0f;

        for (int v = 0; v < NUM_VOICES; v++) {
            if (voices_[v].isActive()) {
                float voiceSample = voices_[v].process();
                // Safety clamp
                if (voiceSample > 1.0f) voiceSample = 1.0f;
                if (voiceSample < -1.0f) voiceSample = -1.0f;

                left += voiceSample * voicePanL_[v];
                right += voiceSample * voicePanR_[v];
            }
        }
        
        // Scale down for mixing
        left *= 0.3f;
        right *= 0.3f;

        // Apply Granular exciter (if enabled) - Parallel additive layer
        if (granularEnabled_) {
            float gran = granular_.process();
            left = left + gran * granularMix_ * 0.3f;
            right = right + gran * granularMix_ * 0.3f;
        }

        // Apply Resonator and Comb (if enabled) - Currently Mono
        if (resonatorEnabled_) {
            float mono = (left + right) * 0.5f;
            float res = resonator_.process(mono);
            // Apply mod wheel to resonator mix
            float resMix = 0.5f + modMatrix_.getModulation(ModDest::RESONATOR_MIX) * 0.5f;
            resMix = constrain(resMix, 0.0f, 1.0f);
            left = left * (1.0f - resMix) + res * resMix;
            right = right * (1.0f - resMix) + res * resMix;
        }
        if (combEnabled_) {
            float mono = (left + right) * 0.5f;
            float cb = comb_.process(mono);
            left = left * 0.5f + cb * 0.5f;
            right = right * 0.5f + cb * 0.5f;
        }

        if (isnan(left) || isinf(left)) left = 0.0f;
        if (isnan(right) || isinf(right)) right = 0.0f;

        // Check which global effects are enabled - skip processing if all disabled
        const bool effectsEnabled = effects_.isSatEnabled() || effects_.isChorusEnabled() || effects_.isDelayEnabled();
        const bool reverbEnabled = reverb_.isEnabled();
        const bool compressorEnabled = compressor_.isEnabled();

        if (effectsEnabled || reverbEnabled || compressorEnabled) {
            // Process global effects - mono for now but we'll adapt them
            float monoInput = (left + right) * 0.5f;
            float wetMono = effectsEnabled ? effects_.process(monoInput) : monoInput;

            // Reverb - let's make it pseudo-stereo
            float wetL = 0.0f, wetR = 0.0f;
            if (reverbEnabled) {
                reverb_.processStereo(monoInput, wetL, wetR);
            }

            float finalL = wetMono * 0.5f + wetL;
            float finalR = wetMono * 0.5f + wetR;

            // Compressor on stereo
            if (compressorEnabled) {
                float compL = compressor_.process(finalL);
                float compR = compressor_.process(finalR);
                finalL = compL;
                finalR = compR;
            }

            // Master volume
            float vol = masterVolume_.process();
            left = finalL * vol;
            right = finalR * vol;
        } else {
            // Skip all global effects - just apply master volume
            float vol = masterVolume_.process();
            left = left * vol;
            right = right * vol;
        }
        
        // Soft clip
        left = fastTanh(left);
        right = fastTanh(right);
        
        blockBuffer_[i * 2] = (int16_t)(left * 32767.0f);
        blockBuffer_[i * 2 + 1] = (int16_t)(right * 32767.0f);
    }
    
    profiler_.endSample();
    size_t bytesWritten;
    esp_err_t err = i2s_write(I2S_NUM_0, blockBuffer_, sizeof(blockBuffer_),
                               &bytesWritten, pdMS_TO_TICKS(10));
    if (err != ESP_OK || bytesWritten != sizeof(blockBuffer_)) {
        i2sUnderrunCount_++;
    }
}

void SynthEngine::audioTaskWrapper(void* param) {
    SynthEngine* engine = static_cast<SynthEngine*>(param);
    while (engine->running_) {
        engine->processBlock();
    }
    vTaskDelete(NULL);
}
