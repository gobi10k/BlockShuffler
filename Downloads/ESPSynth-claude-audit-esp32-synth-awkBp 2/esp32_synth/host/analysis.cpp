#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <complex>
#include <string>

#include "Config.h"
#include "Filter.h"
#include "MoogFilter.h"
#include "Oscillator.h"
#include "Envelope.h"
#include "Reverb.h"
#include "Wavetables.h"

using namespace std;

// -- FFT --
void fft(vector<complex<double>>& a) {
    int n = a.size();
    if (n <= 1) return;
    vector<complex<double>> a0(n / 2), a1(n / 2);
    for (int i = 0; i * 2 < n; i++) {
        a0[i] = a[i * 2];
        a1[i] = a[i * 2 + 1];
    }
    fft(a0);
    fft(a1);
    double ang = 2 * M_PI / n;
    complex<double> w(1, 0), wn(cos(ang), sin(ang));
    for (int i = 0; i * 2 < n; i++) {
        a[i] = a0[i] + w * a1[i];
        a[i + n / 2] = a0[i] - w * a1[i];
        w *= wn;
    }
}

// 1. Filter Sweep
void test_filters() {
    printf("--- 1. FILTER SWEEP ---\n");
    Filter svf;
    MoogFilter moog;
    float resonances[] = {0.0f, 0.3f, 0.6f, 0.9f, 0.99f};
    int n = 8192;
    
    // Test Moog
    for (float res : resonances) {
        moog.setResonance(res);
        moog.setCutoff(1000.0f);
        moog.reset();
        
        vector<complex<double>> ir(n, 0.0);
        ir[0] = 1.0;
        for (int i = 0; i < n; i++) ir[i] = moog.process(ir[i].real());
        fft(ir);
        
        double max_mag = 0; int peak_bin = 0;
        for (int i=0; i<n/2; i++) {
            double mag = abs(ir[i]);
            if (mag > max_mag) { max_mag = mag; peak_bin = i; }
        }
        double peak_freq = peak_bin * SAMPLE_RATE / n;
        
        // Self-osc:
        moog.reset(); moog.process(1.0);
        for(int i=0; i<48000; i++) moog.process(0.0);
        bool osc = abs(moog.process(0.0)) > 0.1;
        
        printf("Moog Res=%.2f: Peak %.1fHz (H=%.2f), Self-osc=%d\n", res, peak_freq, max_mag, osc);
    }
    
    // Test SVF
    printf("\n--- SVF SWEEP ---\n");
    for (float res : resonances) {
        svf.setResonance(res);
        svf.setCutoff(1000.0f);
        
        vector<complex<double>> ir(n, 0.0);
        ir[0] = 1.0;
        // The Chamberlin SVF process updates state. We'll use lowpass output for test.
        svf.setMode(FilterMode::LOWPASS);
        for (int i = 0; i < n; i++) ir[i] = svf.process(ir[i].real());
        fft(ir);
        
        double max_mag = 0; int peak_bin = 0;
        for (int i=0; i<n/2; i++) {
            double mag = abs(ir[i]);
            if (mag > max_mag) { max_mag = mag; peak_bin = i; }
        }
        double peak_freq = peak_bin * SAMPLE_RATE / n;
        
        // Self-osc:
        svf.process(1.0);
        for(int i=0; i<48000; i++) svf.process(0.0);
        bool osc = abs(svf.process(0.0)) > 0.1;
        
        printf("SVF Res=%.2f: Peak %.1fHz (H=%.2f), Self-osc=%d\n", res, peak_freq, max_mag, osc);
    }
}

// 2. Oscillator Spectra
void test_oscillators() {
    printf("\n--- 2. OSCILLATOR SPECTRA ---\n");
    Wavetables::init();
    Oscillator osc;
    int notes[] = {36, 60, 84, 108};
    Waveform waves[] = {Waveform::SAW, Waveform::SQUARE, Waveform::PULSE};
    
    for (Waveform wf : waves) {
        printf("Waveform: %d\n", (int)wf);
        osc.setWaveform(wf);
        for (int note : notes) {
            float freq = 440.0 * pow(2.0, (note - 69) / 12.0);
            osc.setFrequency(freq, true);
            osc.resetPhase();
            
            int n = 16384;
            vector<complex<double>> sig(n, 0.0);
            double dc = 0;
            for (int i = 0; i < n; i++) {
                float v = osc.process();
                sig[i] = v;
                dc += v;
            }
            dc /= n;
            fft(sig);
            
            // THD and Alias energy
            double fundamental_mag = 0;
            double total_harmonic_energy = 0;
            double alias_energy = 0;
            
            int f_bin = round((freq / SAMPLE_RATE) * n);
            if (f_bin > 0 && f_bin < n/2) fundamental_mag = abs(sig[f_bin]);
            
            for (int i=1; i<n/2; i++) {
                double mag = abs(sig[i]);
                if (i != f_bin) total_harmonic_energy += mag*mag;
                // crude alias energy: look at bins near nyquist
                if (i > n*0.4) alias_energy += mag*mag;
            }
            double thd = sqrt(total_harmonic_energy) / (fundamental_mag + 1e-9);
            
            printf("  Note %d (%.1fHz): DC=%.3f, THD=%.2f%%, AliasE=%.2f\n", note, freq, dc, thd*100, alias_energy);
        }
    }
    
    FILE* f = fopen("osc_spectra.csv", "w");
    fprintf(f, "freq,mag\n");
    fclose(f);
}

// 3. Envelope Shape
void test_envelopes() {
    printf("\n--- 3. ENVELOPE SHAPE ---\n");
    Envelope env;
    float attacks[] = {0.001f, 0.010f, 0.100f, 1.000f};
    
    FILE* f = fopen("env_shape.csv", "w");
    fprintf(f, "time_ms,val\n");
    
    for (float a : attacks) {
        env.setADSR(a, a, 0.5f, a);
        env.trigger();
        env.gate(true);
        
        int n = (a * 1.5) * SAMPLE_RATE;
        float half_val = 0; int half_idx = 0;
        for (int i=0; i<n; i++) {
            float v = env.process();
            if (a == 0.1f) fprintf(f, "%.2f,%f\n", i*1000.0/SAMPLE_RATE, v);
            if (v >= 0.5f && half_idx == 0) half_idx = i;
        }
        env.gate(false);
        for(int i=0; i<n; i++) env.process(); // drain release
        
        float attack_lin = (float)half_idx / (a * SAMPLE_RATE);
        printf("Attack %dms: 50%% reached at %.1f%% of A time\n", (int)(a*1000), attack_lin*100);
    }
    fclose(f);
}

// 4. FDN Reverb
void test_reverb() {
    printf("\n--- 4. FDN REVERB ---\n");
    FDNReverb rev;
    rev.setDecay(2.0f);
    rev.setMix(1.0f);
    rev.setSize(0.8f);
    rev.setPreDelay(0);
    
    vector<float> ir(SAMPLE_RATE * 5, 0.0f);
    ir[0] = 1.0f;
    float max_v = 0;
    
    for(int i=0; i<ir.size(); i++) {
        float out1=0, out2=0;
        rev.processStereo(ir[i], out1, out2);
        ir[i] = out1;
        if (i>0 && abs(out1) > max_v) max_v = abs(out1);
    }
    
    // Find RT60
    int t60_idx = 0;
    for (int i = ir.size()-1; i>0; i--) {
        if (abs(ir[i]) > max_v * 0.001) { t60_idx = i; break; }
    }
    printf("Configured decay: 2.0s, Actual RT60: %.2fs\n", (float)t60_idx / SAMPLE_RATE);
}

// 5. Wavetables
void test_wavetables() {
    printf("\n--- 5. WAVETABLES ---\n");
    Wavetables::init();
    
    // Spectral content per octave table
    for (int i=0; i<NUM_OCTAVE_TABLES; i++) {
        int n = WAVETABLE_SIZE;
        vector<complex<double>> s_saw(n, 0.0);
        for (int j=0; j<n; j++) s_saw[j] = Wavetables::sawTables[i][j];
        fft(s_saw);
        
        double max_mag = 0;
        int max_harm = 0;
        for (int k=1; k<n/2; k++) {
            if (abs(s_saw[k]) > 1e-3) max_harm = k;
        }
        printf("Table %d (limit %.1f Hz): max harmonic = %d\n", i, Wavetables::octaveFreqLimits[i], max_harm);
    }
}

int main() {
    test_filters();
    test_oscillators();
    test_envelopes();
    test_reverb();
    test_wavetables();
    return 0;
}
