#include "Analysis.h"
#include "../Oscillator.h"
#include "../Filter.h"
#include "../MoogFilter.h"
#include "../Envelope.h"
#include "../Reverb.h"
#include "../Wavetables.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef HOST_BUILD
#include "platform.h"
#else
#include <Arduino.h>
#endif

constexpr int FFT_SIZE = 8192;
constexpr float ANALYSIS_SAMPLE_RATE = 48000.0f;

float midiNoteToFreq(int note) {
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

void generateSine(std::vector<float>& buffer, float freq, float duration, float sampleRate) {
    int numSamples = (int)(duration * sampleRate);
    buffer.resize(numSamples);
    for (int i = 0; i < numSamples; i++) {
        buffer[i] = sinf(2.0f * M_PI * freq * i / sampleRate);
    }
}

void applyHannWindow(std::vector<float>& buffer) {
    int n = buffer.size();
    for (int i = 0; i < n; i++) {
        float w = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (n - 1)));
        buffer[i] *= w;
    }
}

void computeFFTInternal(const std::vector<float>& input, std::vector<float>& real, std::vector<float>& imag) {
    int n = input.size();
    real = input;
    imag.resize(n, 0.0f);
    
    int m = 1;
    while (m < n) m <<= 1;
    real.resize(m, 0.0f);
    imag.resize(m, 0.0f);
    
    int j = 0;
    for (int i = 0; i < m - 1; i++) {
        if (i < j) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
        int k = m / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }
    
    for (int len = 2; len <= m; len <<= 1) {
        float angle = -2.0f * M_PI / len;
        float wr = 1.0f, wi = 0.0f;
        float tr = cosf(angle), ti = sinf(angle);
        for (int i = 0; i < len / 2; i++) {
            for (int k = i; k < m; k += len) {
                int l = k + len / 2;
                float xr = real[l] * tr - imag[l] * ti;
                float xi = real[l] * tr + imag[l] * tr;
                real[l] = real[k] - xr;
                imag[l] = imag[k] - xi;
                real[k] += xr;
                imag[k] += xi;
            }
            float tmp = wr;
            wr = wr * tr - wi * ti;
            wi = tmp * tr + wi * ti;
        }
    }
}

static std::vector<float> computeMagnitude(const std::vector<float>& real, const std::vector<float>& imag) {
    int n = real.size();
    std::vector<float> mag(n / 2);
    for (int i = 0; i < n / 2; i++) {
        mag[i] = sqrtf(real[i] * real[i] + imag[i] * imag[i]);
    }
    return mag;
}

std::vector<float> computeFFT(const std::vector<float>& samples) {
    std::vector<float> windowed = samples;
    applyHannWindow(windowed);
    
    std::vector<float> real, imag;
    computeFFTInternal(windowed, real, imag);
    
    return computeMagnitude(real, imag);
}

float findMinus3dBPoint(const std::vector<float>& magnitude) {
    float maxMag = 0.0f;
    for (float m : magnitude) if (m > maxMag) maxMag = m;
    if (maxMag == 0.0f) return 0.0f;
    
    float target = maxMag * 0.707f;
    for (size_t i = 1; i < magnitude.size(); i++) {
        if (magnitude[i] <= target && magnitude[i-1] > target) {
            return (float)i / magnitude.size();
        }
    }
    return 1.0f;
}

float findResonancePeak(const std::vector<float>& magnitude) {
    float maxMag = 0.0f;
    for (float m : magnitude) if (m > maxMag) maxMag = m;
    
    float dcMag = magnitude.size() > 0 ? magnitude[0] : 0.0f;
    return maxMag / (dcMag > 0.001f ? dcMag : 1.0f);
}

float computeTHD(const std::vector<float>& spectrum, int fundamentalBin) {
    if (fundamentalBin <= 0 || fundamentalBin >= (int)spectrum.size() - 1) return 0.0f;
    
    float fundamentalPower = spectrum[fundamentalBin] * spectrum[fundamentalBin];
    if (fundamentalPower < 1e-10f) return 0.0f;
    
    float harmPower = 0.0f;
    int startBin = max(0, fundamentalBin - 2);
    int endBin = min((int)spectrum.size() - 1, fundamentalBin + 2);
    
    for (int i = 0; i < startBin; i++) {
        harmPower += spectrum[i] * spectrum[i];
    }
    for (int i = endBin + 1; i < (int)spectrum.size(); i++) {
        harmPower += spectrum[i] * spectrum[i];
    }
    
    return sqrtf(harmPower / fundamentalPower);
}

float computeDC(const std::vector<float>& samples) {
    float sum = 0.0f;
    for (float s : samples) sum += s;
    return sum / samples.size();
}

float computeRT60(const std::vector<float>& impulse, float sampleRate) {
    float impulsePower = 0.0f;
    for (float s : impulse) impulsePower += s * s;
    if (impulsePower < 1e-10f) return 0.0f;
    
    float threshold = impulsePower * 0.001f;
    float crossTime = -1.0f;
    
    for (size_t i = 0; i < impulse.size(); i++) {
        float envPower = 0.0f;
        size_t window = min((size_t)500, impulse.size() - i);
        for (size_t j = 0; j < window; j++) {
            envPower += impulse[i + j] * impulse[i + j];
        }
        
        if (envPower < threshold) {
            crossTime = (float)i / sampleRate;
            break;
        }
    }
    
    return crossTime > 0.0f ? crossTime * 1.0f : (float)impulse.size() / sampleRate;
}

float computeEchoDensity(const std::vector<float>& impulse) {
    int numBins = min(100, (int)impulse.size() / 10);
    float growth = 0.0f;
    for (int i = 1; i < numBins; i++) {
        int count = 0;
        for (int j = i * 10; j < (i + 1) * 10 && j < (int)impulse.size(); j++) {
            if (fabsf(impulse[j]) > 0.01f) count++;
        }
        growth += count;
    }
    return growth / numBins;
}

void writeCSV(const std::string& filename, const std::vector<float>& data) {
    FILE* f = fopen(filename.c_str(), "w");
    if (!f) return;
    fprintf(f, "index,value\n");
    for (size_t i = 0; i < data.size(); i++) {
        fprintf(f, "%zu,%.6f\n", i, data[i]);
    }
    fclose(f);
}

void writeCSV2D(const std::string& filename, 
                    const std::vector<std::pair<float, float>>& data) {
    FILE* f = fopen(filename.c_str(), "w");
    if (!f) return;
    fprintf(f, "x,y\n");
    for (auto& p : data) {
        fprintf(f, "%.2f,%.6f\n", p.first, p.second);
    }
    fclose(f);
}

void Analysis::runAll() {
    Serial.println("\n=== RUNNING DSP ANALYSIS ===");
    testFilterSweep();
    testOscillatorSpectra();
    testEnvelopeShape();
    testReverbImpulse();
    testWavetableSpectral();
    Serial.println("=== ANALYSIS COMPLETE ===\n");
}

void Analysis::testFilterSweep() {
    Serial.println("\n--- FILTER SWEEP ANALYSIS ---");
    
    const float resonances[] = {0.0f, 0.3f, 0.6f, 0.9f, 0.99f};
    const int numRes = sizeof(resonances) / sizeof(resonances[0]);
    
    float cutoffMin = 20.0f;
    float cutoffMax = 20000.0f;
    int numCutoffs = 20;
    
    struct FilterResult {
        float resonance;
        float cutoffHz;
        float minus3dB;
        float peakGain;
        bool unstable;
    };
    std::vector<FilterResult> svfResults, ladderResults;
    
    for (int r = 0; r < numRes; r++) {
        for (int c = 0; c < numCutoffs; c++) {
            float cutoff = cutoffMin * powf(cutoffMax / cutoffMin, (float)c / (numCutoffs - 1));
            
            Filter svf;
            svf.setCutoff(cutoff);
            svf.setResonance(resonances[r]);
            svf.setMode(FilterMode::LOWPASS);
            
            LadderFilter ladder;
            ladder.setCutoff(cutoff);
            ladder.setResonance(resonances[r]);
            ladder.setMode(LadderMode::LP24);
            
            std::vector<float> impulse;
            impulse.reserve(4096);
            for (int i = 0; i < 4096; i++) {
                float in = (i == 0) ? 1.0f : 0.0f;
                impulse.push_back(svf.process(in));
            }
            
            std::vector<float> mag = computeFFT(impulse);
            
            FilterResult sr;
            sr.resonance = resonances[r];
            sr.cutoffHz = cutoff;
            sr.minus3dB = findMinus3dBPoint(mag);
            sr.peakGain = findResonancePeak(mag);
            sr.unstable = false;
            for (float s : impulse) {
                if (isnan(s) || isinf(s) || fabsf(s) > 10.0f) {
                    sr.unstable = true;
                    break;
                }
            }
            svfResults.push_back(sr);
            
            impulse.clear();
            for (int i = 0; i < 4096; i++) {
                float in = (i == 0) ? 1.0f : 0.0f;
                impulse.push_back(ladder.process(in));
            }
            
            mag = computeFFT(impulse);
            
            FilterResult lr;
            lr.resonance = resonances[r];
            lr.cutoffHz = cutoff;
            lr.minus3dB = findMinus3dBPoint(mag);
            lr.peakGain = findResonancePeak(mag);
            lr.unstable = false;
            for (float s : impulse) {
                if (isnan(s) || isinf(s) || fabsf(s) > 10.0f) {
                    lr.unstable = true;
                    break;
                }
            }
            ladderResults.push_back(lr);
        }
    }
    
    Serial.println("SVF Results:");
    for (auto& r : svfResults) {
        Serial.printf("  Q=%.2f fc=%.0fHz -3dB@%.2f peak=%.2f unstable=%d\n",
                    r.resonance, r.cutoffHz, r.minus3dB * SAMPLE_RATE / 2.0f, r.peakGain, r.unstable);
    }
    
    Serial.println("\nLadder Results:");
    for (auto& r : ladderResults) {
        Serial.printf("  Q=%.2f fc=%.0fHz -3dB@%.2f peak=%.2f unstable=%d\n",
                    r.resonance, r.cutoffHz, r.minus3dB * SAMPLE_RATE / 2.0f, r.peakGain, r.unstable);
    }
    
    std::vector<std::pair<float, float>> svfCsv, ladderCsv;
    for (auto& r : svfResults) {
        svfCsv.push_back({r.cutoffHz, r.minus3dB * SAMPLE_RATE / 2.0f});
    }
    for (auto& r : ladderResults) {
        ladderCsv.push_back({r.cutoffHz, r.minus3dB * SAMPLE_RATE / 2.0f});
    }
    
    writeCSV2D("filter_svf_sweep.csv", svfCsv);
    writeCSV2D("filter_ladder_sweep.csv", ladderCsv);
    
    float selfOscThreshold = -1.0f;
    for (int i = svfResults.size() - 1; i >= 0; i--) {
        if (svfResults[i].peakGain > 10.0f) {
            selfOscThreshold = svfResults[i].resonance;
            break;
        }
    }
    
    Serial.printf("\nFlags:\n");
    if (selfOscThreshold > 0.0f) {
        Serial.printf("  SVF self-oscillation threshold: Q=%.2f\n", selfOscThreshold);
    } else {
        Serial.printf("  SVF: No self-oscillation detected\n");
    }
}

void Analysis::testOscillatorSpectra() {
    Serial.println("\n--- OSCILLATOR SPECTRA ANALYSIS ---");
    
    const int midiNotes[] = {36, 60, 84, 108};
    const Waveform waveforms[] = {Waveform::SINE, Waveform::SAW, Waveform::SQUARE, Waveform::TRIANGLE};
    const char* wfNames[] = {"SINE", "SAW", "SQUARE", "TRIANGLE"};
    
    float nyquist = SAMPLE_RATE / 2.0f;
    float aliasThreshold = nyquist / 2.0f;
    
    for (int w = 0; w < 4; w++) {
        Serial.printf("Waveform: %s\n", wfNames[w]);
        
        for (int note : midiNotes) {
            float freq = midiNoteToFreq(note);
            
            if (freq > nyquist * 0.9f) {
                Serial.printf("  Note %d (%.1fHz): SKIP (above Nyquist)\n", note, freq);
                continue;
            }
            
            Oscillator osc;
            osc.setFrequency(freq);
            osc.setWaveform(waveforms[w]);
            
            std::vector<float> samples;
            int numSamples = (int)(SAMPLE_RATE * 4.0f);
            samples.reserve(numSamples);
            
            for (int i = 0; i < numSamples; i++) {
                samples.push_back(osc.process());
            }
            
            std::vector<float> spectrum = computeFFT(samples);
            
            int fundamentalBin = (int)(freq * FFT_SIZE / SAMPLE_RATE);
            if (fundamentalBin >= 0 && fundamentalBin < (int)spectrum.size()) {
                float aliasEnergy = 0.0f;
                int aliasStart = (int)(aliasThreshold * FFT_SIZE / SAMPLE_RATE);
                for (int i = aliasStart; i < (int)spectrum.size(); i++) {
                    aliasEnergy += spectrum[i] * spectrum[i];
                }
                
                float totalEnergy = 0.0f;
                for (float s : spectrum) totalEnergy += s * s;
                
                float thd = computeTHD(spectrum, fundamentalBin);
                float dc = computeDC(samples);
                
                Serial.printf("  Note %d (%.1fHz): alias=%.4f THD=%.4f DC=%.6f\n",
                            note, freq, aliasEnergy / totalEnergy, thd, dc);
            }
        }
    }
    
    Serial.println("\nFlags:");
    Serial.printf("  High-note aliasing: Check notes above MIDI 96\n");
}

void Analysis::testEnvelopeShape() {
    Serial.println("\n--- ENVELOPE SHAPE ANALYSIS ---");
    
    const float attackTimes[] = {0.001f, 0.01f, 0.1f, 1.0f};
    const int numTimes = sizeof(attackTimes) / sizeof(attackTimes[0]);
    
    for (int i = 0; i < numTimes; i++) {
        float a = attackTimes[i];
        float d = attackTimes[i];
        float s = 0.5f;
        float r = attackTimes[i];
        
        Envelope env;
        env.setADSR(a, d, s, r);
        env.gate(true);
        
        std::vector<float> samples;
        int expectedDuration = (int)((a + d + 1.0f + r) * SAMPLE_RATE);
        samples.reserve(expectedDuration + 1000);
        
        for (int j = 0; j < expectedDuration + 1000; j++) {
            samples.push_back(env.process());
            if (env.getStage() == EnvelopeStage::SUSTAIN && j > (int)((a + d) * SAMPLE_RATE + 100)) {
                break;
            }
        }
        
        int attackEnd = (int)(a * SAMPLE_RATE);
        int decayEnd = (int)((a + d) * SAMPLE_RATE);
        
        float attackSlope = 0.0f;
        if (attackEnd > 1) {
            float startVal = samples[0];
            float endVal = samples[min(attackEnd, (int)samples.size() - 1)];
            attackSlope = (endVal - startVal) / a;
        }
        
        float releaseSlope = 0.0f;
        if (samples.size() > decayEnd + 100) {
            int releaseStart = decayEnd + (int)(SAMPLE_RATE * 0.5f);
            int releaseEnd = min((int)samples.size() - 1, decayEnd + (int)(SAMPLE_RATE * (a + 0.5f)));
            if (releaseEnd > releaseStart && releaseEnd < (int)samples.size()) {
                releaseSlope = (samples[releaseEnd] - samples[releaseStart]) / r;
            }
        }
        
        Serial.printf("  A=%.0fms: attack_slope=%.2f release_slope=%.2f coef=%.4f\n",
                    a * 1000.0f, attackSlope, releaseSlope, env.getValue());
        
        writeCSV("envelope_" + std::to_string((int)(a * 1000)) + "ms.csv", samples);
    }
    
    Serial.println("\nFlags:");
    Serial.printf("  Check linearity: slope should be ~1/tau for ideal one-pole\n");
    Serial.printf("  Check release: exponential decay vs linear falloff\n");
}

void Analysis::testReverbImpulse() {
    Serial.println("\n--- FDN REVERB ANALYSIS ---");
    
    FDNReverb reverb;
    reverb.setEnabled(true);
    reverb.setDecay(3.0f);
    reverb.setSize(0.8f);
    reverb.setDamping(0.3f);
    reverb.setMix(1.0f);
    
    int numSamples = (int)(10.0f * SAMPLE_RATE);
    std::vector<float> impulse;
    impulse.reserve(numSamples);
    
    for (int i = 0; i < numSamples; i++) {
        float in = (i == 0) ? 1.0f : 0.0f;
        float out = reverb.process(in);
        impulse.push_back(out);
    }
    
    float configuredRT60 = 3.0f;
    float measuredRT60 = computeRT60(impulse, SAMPLE_RATE);
    
    float echoDensity = computeEchoDensity(impulse);
    
    float lowFreqDecay = 0.0f, highFreqDecay = 0.0f;
    int quarter = numSamples / 4;
    for (int i = 0; i < quarter; i++) {
        lowFreqDecay += impulse[i] * impulse[i];
    }
    for (int i = quarter * 3; i < numSamples; i++) {
        highFreqDecay += impulse[i] * impulse[i];
    }
    
    int numModes = 0;
    for (int i = 1; i < numSamples - 1; i++) {
        if (impulse[i-1] < 0.0f && impulse[i] >= 0.0f && fabsf(impulse[i]) > 0.001f) {
            numModes++;
        }
    }
    
    Serial.printf("  Configured RT60: %.2fs\n", configuredRT60);
    Serial.printf("  Measured RT60: %.2fs\n", measuredRT60);
    Serial.printf("  Echo density: %.2f peaks/sample\n", echoDensity);
    Serial.printf("  Freq-dependent decay: low=%.2f high=%.2f ratio=%.2f\n",
                  lowFreqDecay, highFreqDecay, highFreqDecay / (lowFreqDecay + 1e-6f));
    Serial.printf("  Modal density: %d zero crossings\n", numModes);
    
    writeCSV("reverb_impulse.csv", impulse);
    
    Serial.println("\nFlags:");
    if (fabsf(measuredRT60 - configuredRT60) > 0.5f) {
        Serial.printf("  RT60 mismatch: expected %.2fs, got %.2fs\n", configuredRT60, measuredRT60);
    }
    if (echoDensity < 1.0f) {
        Serial.printf("  Low echo density: %.2f (expect >1 for good diffusion)\n", echoDensity);
    }
}

void Analysis::testWavetableSpectral() {
    Serial.println("\n--- WAVETABLE SPECTRAL ANALYSIS ---");
    
    Wavetables::init();
    
    float tableThresholds[] = {65.4f * 2.0f, 130.8f * 2.0f, 261.6f * 2.0f, 
                              523.3f * 2.0f, 1046.5f * 2.0f, 2093.0f * 2.0f};
    
    for (int t = 0; t < NUM_OCTAVE_TABLES; t++) {
        Serial.printf("Table %d (limit %.0fHz):\n", t, tableThresholds[t]);
        
        float freqBelow = tableThresholds[t] * 0.99f;
        float freqAbove = tableThresholds[t] * 1.01f;
        
        for (float freq : {freqBelow, freqAbove}) {
            if (freq > SAMPLE_RATE * 0.45f) {
                Serial.printf("  %.0fHz: SKIP (alias risk)\n", freq);
                continue;
            }
            
            Oscillator osc;
            osc.setFrequency(freq);
            osc.setWaveform(Waveform::SAW);
            
            std::vector<float> samples;
            int numSamples = (int)(SAMPLE_RATE * 0.5f);
            samples.reserve(numSamples);
            
            for (int i = 0; i < numSamples; i++) {
                samples.push_back(osc.process());
            }
            
            std::vector<float> spectrum = computeFFT(samples);
            
            float aliasEnergy = 0.0f;
            float nyquist = SAMPLE_RATE / 2.0f;
            int aliasStart = (int)(nyquist * 0.75f * FFT_SIZE / SAMPLE_RATE);
            for (int i = aliasStart; i < (int)spectrum.size(); i++) {
                aliasEnergy += spectrum[i] * spectrum[i];
            }
            
            float totalEnergy = 0.0f;
            for (float s : spectrum) totalEnergy += s * s;
            
            Serial.printf("  %.0fHz: alias=%.4f\n", freq, aliasEnergy / totalEnergy);
        }
    }
    
    Serial.println("\nFlags:");
    Serial.printf("  Check transition: higher alias energy at table boundary\n");
}