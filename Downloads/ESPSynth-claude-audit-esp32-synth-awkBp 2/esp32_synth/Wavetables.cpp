#include "Wavetables.h"
#include <math.h>

const char* WAVEFORM_NAMES[] = {"SIN", "SAW", "SQR", "TRI", "PLS", "SUP", "NOI"};

namespace Wavetables {

    float sawTables[NUM_OCTAVE_TABLES][WAVETABLE_SIZE];
    float squareTables[NUM_OCTAVE_TABLES][WAVETABLE_SIZE];
    float triangleTables[NUM_OCTAVE_TABLES][WAVETABLE_SIZE];
    float sinTable[WAVETABLE_SIZE];
    float octaveFreqLimits[NUM_OCTAVE_TABLES];

    namespace {
        
        inline int maxHarmonics(float baseFreq) {
            return (int)((SAMPLE_RATE * 0.5f) / baseFreq);
        }
        
        inline float lanczos(int harmonic, int maxHarm) {
            if (harmonic == 0) return 1.0f;
            float x = M_PI * harmonic / maxHarm;
            return sinf(x) / x;
        }
        
        void normalizeTable(float* table) {
            float maxVal = 0.0f;
            for (int i = 0; i < WAVETABLE_SIZE; i++) {
                if (fabsf(table[i]) > maxVal) maxVal = fabsf(table[i]);
            }
            if (maxVal > 0.0f) {
                float invMax = 1.0f / maxVal;
                for (int i = 0; i < WAVETABLE_SIZE; i++) {
                    table[i] *= invMax;
                }
            }
        }
        
        inline float lerp(float a, float b, float t) {
            return a + t * (b - a);
        }
        
        inline void phaseToIndex(uint32_t phase, uint16_t& idx0, uint16_t& idx1, float& frac) {
            uint32_t tablePos = phase >> (PHASE_BITS - WAVETABLE_BITS);
            uint32_t fracBits = (phase >> (PHASE_BITS - WAVETABLE_BITS - 16)) & 0xFFFF;
            idx0 = tablePos & WAVETABLE_MASK;
            idx1 = (tablePos + 1) & WAVETABLE_MASK;
            frac = fracBits * (1.0f / 65536.0f);
        }
        
        void generateSawTable(float* table, int maxHarm) {
            for (int i = 0; i < WAVETABLE_SIZE; i++) table[i] = 0.0f;
            for (int h = 1; h <= maxHarm; h++) {
                float sigma = lanczos(h, maxHarm);
                float amplitude = sigma / h;
                for (int i = 0; i < WAVETABLE_SIZE; i++) {
                    float phase = (2.0f * M_PI * h * i) / WAVETABLE_SIZE;
                    table[i] += amplitude * sinf(phase);
                }
            }
            normalizeTable(table);
        }
        
        void generateSquareTable(float* table, int maxHarm) {
            for (int i = 0; i < WAVETABLE_SIZE; i++) table[i] = 0.0f;
            for (int h = 1; h <= maxHarm; h += 2) {
                float sigma = lanczos(h, maxHarm);
                float amplitude = sigma / h;
                for (int i = 0; i < WAVETABLE_SIZE; i++) {
                    float phase = (2.0f * M_PI * h * i) / WAVETABLE_SIZE;
                    table[i] += amplitude * sinf(phase);
                }
            }
            normalizeTable(table);
        }
        
        void generateTriangleTable(float* table, int maxHarm) {
            for (int i = 0; i < WAVETABLE_SIZE; i++) table[i] = 0.0f;
            int sign = 1;
            for (int h = 1; h <= maxHarm; h += 2) {
                float sigma = lanczos(h, maxHarm);
                float amplitude = sigma * sign / (float)(h * h);
                for (int i = 0; i < WAVETABLE_SIZE; i++) {
                    float phase = (2.0f * M_PI * h * i) / WAVETABLE_SIZE;
                    table[i] += amplitude * sinf(phase);
                }
                sign = -sign;
            }
            normalizeTable(table);
        }
        
    }

    void init() {
        Serial.println("[Wavetables] Generating...");
        
        for (int i = 0; i < WAVETABLE_SIZE; i++) {
            sinTable[i] = sinf((2.0f * M_PI * i) / WAVETABLE_SIZE);
        }
        
        // 6 octave tables: C2, C3, C4, C5, C6, C7
        float baseFreqs[NUM_OCTAVE_TABLES] = {65.4f, 130.8f, 261.6f, 523.3f, 1046.5f, 2093.0f};
        
        for (int oct = 0; oct < NUM_OCTAVE_TABLES; oct++) {
            octaveFreqLimits[oct] = baseFreqs[oct] * 2.0f;
        }
        
        for (int oct = 0; oct < NUM_OCTAVE_TABLES; oct++) {
            // Calculate max harmonics based on the HIGHEST frequency that will use this table
            // to avoid aliasing at the top of the octave range.
            int maxHarm = maxHarmonics(octaveFreqLimits[oct]);
            // Also clamp to 127 because WAVETABLE_SIZE is 256 (Nyquist limit of the table itself)
            maxHarm = min(maxHarm, 127);
            
            Serial.printf("[Wavetables] Table %d: %.0fHz limit, %d harmonics\n", 
                         oct, octaveFreqLimits[oct], maxHarm);
            
            generateSawTable(sawTables[oct], maxHarm);
            generateSquareTable(squareTables[oct], maxHarm);
            generateTriangleTable(triangleTables[oct], maxHarm);
        }
        
        Serial.println("[Wavetables] Ready");
    }

    int tableIndexForFreq(float freq) {
        for (int i = 0; i < NUM_OCTAVE_TABLES - 1; i++) {
            if (freq < octaveFreqLimits[i]) return i;
        }
        return NUM_OCTAVE_TABLES - 1;
    }

    float readSine(uint32_t phase) {
        uint16_t idx0, idx1;
        float frac;
        phaseToIndex(phase, idx0, idx1, frac);
        return lerp(sinTable[idx0], sinTable[idx1], frac);
    }

    float readSaw(uint32_t phase, int tableIndex) {
        if (tableIndex < 0) tableIndex = 0;
        if (tableIndex >= NUM_OCTAVE_TABLES) tableIndex = NUM_OCTAVE_TABLES - 1;
        uint16_t idx0, idx1;
        float frac;
        phaseToIndex(phase, idx0, idx1, frac);
        return lerp(sawTables[tableIndex][idx0], sawTables[tableIndex][idx1], frac);
    }

    float readSquare(uint32_t phase, int tableIndex) {
        if (tableIndex < 0) tableIndex = 0;
        if (tableIndex >= NUM_OCTAVE_TABLES) tableIndex = NUM_OCTAVE_TABLES - 1;
        uint16_t idx0, idx1;
        float frac;
        phaseToIndex(phase, idx0, idx1, frac);
        return lerp(squareTables[tableIndex][idx0], squareTables[tableIndex][idx1], frac);
    }

    float readTriangle(uint32_t phase, int tableIndex) {
        if (tableIndex < 0) tableIndex = 0;
        if (tableIndex >= NUM_OCTAVE_TABLES) tableIndex = NUM_OCTAVE_TABLES - 1;
        uint16_t idx0, idx1;
        float frac;
        phaseToIndex(phase, idx0, idx1, frac);
        return lerp(triangleTables[tableIndex][idx0], triangleTables[tableIndex][idx1], frac);
    }

}
