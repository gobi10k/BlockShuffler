#ifndef ANALYSIS_H
#define ANALYSIS_H

#include <string>
#include <vector>
#include <utility>

class Analysis {
public:
    static void runAll();

private:
    static void testFilterSweep();
    static void testOscillatorSpectra();
    static void testEnvelopeShape();
    static void testReverbImpulse();
    static void testWavetableSpectral();
};

// Helper functions - implemented in Analysis.cpp
float midiNoteToFreq(int midiNote);
std::vector<float> computeFFT(const std::vector<float>& samples);
float findMinus3dBPoint(const std::vector<float>& magnitude);
float findResonancePeak(const std::vector<float>& magnitude);
float computeTHD(const std::vector<float>& spectrum, int fundamentalBin);
float computeDC(const std::vector<float>& samples);
float computeRT60(const std::vector<float>& impulse, float sampleRate);
float computeEchoDensity(const std::vector<float>& impulse);
void writeCSV(const std::string& filename, const std::vector<float>& data);
void writeCSV2D(const std::string& filename,
                const std::vector<std::pair<float, float>>& data);

#endif