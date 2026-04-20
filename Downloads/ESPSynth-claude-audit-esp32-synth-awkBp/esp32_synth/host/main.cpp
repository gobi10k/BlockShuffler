// host/main.cpp — desktop render entry point.
// Drives SynthEngine with a hardcoded note sequence and writes out.wav.
// Usage: ./espsynth_host [analysis|synth]

#include "SynthEngine.h"
#include "Analysis.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// ── Minimal RIFF/WAV header writer ───────────────────────────────────────────
static void write_wav_header(FILE* f, int channels, int sample_rate,
                              int bits_per_sample, int num_samples) {
    auto write32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto write16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };

    uint32_t data_size   = (uint32_t)(num_samples * channels * bits_per_sample / 8);
    uint32_t riff_size   = 36 + data_size;
    uint32_t byte_rate   = (uint32_t)(sample_rate * channels * bits_per_sample / 8);
    uint16_t block_align = (uint16_t)(channels * bits_per_sample / 8);

    fwrite("RIFF", 1, 4, f);  write32(riff_size);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);  write32(16);        // PCM chunk size
    write16(1);                                     // PCM format
    write16((uint16_t)channels);
    write32((uint32_t)sample_rate);
    write32(byte_rate);
    write16(block_align);
    write16((uint16_t)bits_per_sample);
    fwrite("data", 1, 4, f);  write32(data_size);
}

// ── Note event list ───────────────────────────────────────────────────────────
struct Event { float t; bool on; uint8_t note, vel; };

static const Event EVENTS[] = {
    // C major arpeggio
    {0.00f, true,  60, 100}, {0.22f, false, 60, 0},
    {0.30f, true,  64,  95}, {0.52f, false, 64, 0},
    {0.60f, true,  67, 100}, {0.82f, false, 67, 0},
    {0.90f, true,  72, 110}, {1.12f, false, 72, 0},
    // F major arpeggio
    {1.25f, true,  65,  95}, {1.47f, false, 65, 0},
    {1.55f, true,  69,  90}, {1.77f, false, 69, 0},
    {1.85f, true,  72, 100}, {2.07f, false, 72, 0},
    {2.15f, true,  77, 105}, {2.37f, false, 77, 0},
    // G major arpeggio
    {2.50f, true,  67, 100}, {2.72f, false, 67, 0},
    {2.80f, true,  71,  95}, {3.02f, false, 71, 0},
    {3.10f, true,  74, 100}, {3.32f, false, 74, 0},
    {3.40f, true,  79, 110}, {3.62f, false, 79, 0},
    // C major chord
    {3.75f, true,  60, 100},
    {3.75f, true,  64,  95},
    {3.75f, true,  67, 100},
    {4.50f, false, 60, 0},
    {4.50f, false, 64, 0},
    {4.50f, false, 67, 0},
};
static const int NUM_EVENTS = (int)(sizeof(EVENTS) / sizeof(EVENTS[0]));

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "analysis") == 0) {
        Analysis::runAll();
        return 0;
    }

    SynthEngine engine;

    if (!engine.init()) {
        fprintf(stderr, "SynthEngine::init() failed\n");
        return 1;
    }

    // Patch parameters — matching a warm pad sound.
    engine.setOscWaveform(0, Waveform::SAW);
    engine.setOscWaveform(1, Waveform::SAW);
    engine.setOscDetune(1, 7.0f);           // osc 2 slightly detuned
    engine.setOscMix(0.5f);
    engine.setFilterCutoff(2400.0f);
    engine.setFilterResonance(0.35f);
    engine.setFilterMode(FilterMode::LOWPASS);
    engine.setAmpADSR(0.02f, 0.18f, 0.70f, 0.45f);
    engine.setFilterADSR(0.01f, 0.30f, 0.40f, 0.60f);
    engine.setFilterEnvAmount(0.4f);
    engine.setMasterVolume(0.75f);

    engine.getReverb().setDecay(1.8f);
    engine.getReverb().setMix(0.38f);
    engine.getReverb().setEnabled(true);

    // Render
    const float TOTAL_SECONDS = 6.0f;   // includes reverb tail
    const int TOTAL_BLOCKS    = (int)(TOTAL_SECONDS * SAMPLE_RATE
                                      / DMA_BUFFER_SAMPLES) + 1;
    const float DT            = (float)DMA_BUFFER_SAMPLES / SAMPLE_RATE;
    const int TOTAL_SAMPLES   = TOTAL_BLOCKS * DMA_BUFFER_SAMPLES;

    FILE* f = fopen("out.wav", "wb");
    if (!f) { perror("out.wav"); return 1; }

    write_wav_header(f, 2, SAMPLE_RATE, 16, TOTAL_SAMPLES);

    int ev = 0;
    for (int b = 0; b < TOTAL_BLOCKS; b++) {
        float t = b * DT;
        while (ev < NUM_EVENTS && EVENTS[ev].t <= t) {
            if (EVENTS[ev].on)
                engine.noteOn(EVENTS[ev].note, EVENTS[ev].vel);
            else
                engine.noteOff(EVENTS[ev].note);
            ++ev;
        }
        engine.renderBlock();
        fwrite(engine.getBlockBuffer(), sizeof(int16_t),
               DMA_BUFFER_SAMPLES * 2, f);
    }

    fclose(f);
    printf("Wrote out.wav  — %d blocks, %.1f s, 48 kHz 16-bit stereo PCM\n",
           TOTAL_BLOCKS, TOTAL_SECONDS);
    return 0;
}
