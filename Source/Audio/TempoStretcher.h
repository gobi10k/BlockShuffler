#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>

namespace BlockShuffler {

/**
 * Pitch-preserving time stretcher using WSOLA (Waveform Similarity Overlap-Add).
 *
 * stretchRatio > 1  →  output is LONGER  (slowed down, pitch unchanged)
 * stretchRatio < 1  →  output is SHORTER (sped up,    pitch unchanged)
 *
 * Also provides a simple linear-interpolation resampler (resampleAdd) for
 * sample-rate conversion where pitch change IS acceptable.
 */
struct TempoStretcher
{
    /**
     * Offline WSOLA stretch. Returns a new AudioBuffer containing srcSamples
     * samples from src starting at srcStart, time-stretched by stretchRatio.
     * Output length = round(srcSamples * stretchRatio).
     */
    static juce::AudioBuffer<float> stretch(
        const juce::AudioBuffer<float>& src,
        int srcStart,
        int srcSamples,
        float stretchRatio)
    {
        if (stretchRatio <= 0.0f || srcSamples <= 0) return {};

        const int srcLen  = src.getNumSamples();
        const int numCh   = src.getNumChannels();
        const int outLen  = juce::jmax(1, (int)((float)srcSamples * stretchRatio + 0.5f));
        if (numCh == 0 || srcLen == 0) return {};

        juce::AudioBuffer<float> out(numCh, outLen);
        out.clear();

        std::vector<float> windowAcc((size_t)outLen, 0.0f);

        // WSOLA parameters
        const int frameSize   = 2048;
        const int analysisHop = 512;
        const int searchRange = 256;
        const int overlapLen  = juce::jmax(0, frameSize - analysisHop);

        // Hanning window
        std::vector<float> window((size_t)frameSize);
        for (int i = 0; i < frameSize; ++i)
            window[(size_t)i] = 0.5f * (1.0f - std::cos(
                2.0f * 3.14159265358979323846f * (float)i / (float)(frameSize - 1)));

        // Windowed tail of previously placed frame for correlation (all channels)
        juce::AudioBuffer<float> prevOverlap(numCh, juce::jmax(1, overlapLen));
        prevOverlap.clear();
        bool havePrev = false;

        int nominalPos  = srcStart;
        int outputPos   = 0;     // position in output buffer (0..outLen-1)
        int synthesisPos = 0;   // position in source for overlap-add (source-relative)

        while (outputPos < outLen)
        {
            // 1. Find best analysis position by normalized cross-correlation
            int actualPos = juce::jlimit(0, juce::jmax(0, srcLen - 1), nominalPos);

            if (havePrev && overlapLen > 0)
            {
                float bestCorr = -1.0e30f;
                float na = 0.0f;
                for (int ch = 0; ch < numCh; ++ch)
                {
                    const float* po = prevOverlap.getReadPointer(ch);
                    for (int k = 0; k < overlapLen; ++k)
                        na += po[k] * po[k];
                }

                if (na > 1.0e-10f)
                {
                    for (int delta = -searchRange; delta <= searchRange; ++delta)
                    {
                        int testPos = nominalPos + delta;
                        if (testPos < 0 || testPos + overlapLen > srcLen) continue;

                        float corr = 0.0f, nb = 0.0f;
                        for (int ch = 0; ch < numCh; ++ch)
                        {
                            const float* rd = src.getReadPointer(ch) + testPos;
                            const float* po = prevOverlap.getReadPointer(ch);
                            for (int k = 0; k < overlapLen; ++k)
                            {
                                corr += po[k] * rd[k];
                                nb   += rd[k] * rd[k];
                            }
                        }
                        float norm = std::sqrt(na * nb);
                        if (norm > 1.0e-10f) corr /= norm;

                        if (corr > bestCorr) { bestCorr = corr; actualPos = testPos; }
                    }
                }
            }

            // 2. Overlap-add windowed frame into output.
            // Clamp to both output bounds and source bounds.
            int count = juce::jmin(frameSize, outLen - outputPos);
            count = juce::jmin(count, srcLen - actualPos);
            if (count <= 0) break;

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float* rd = src.getReadPointer(ch) + actualPos;
                float* wr = out.getWritePointer(ch) + outputPos;
                for (int k = 0; k < count; ++k)
                    wr[k] += rd[k] * window[(size_t)k];
            }
            for (int k = 0; k < count; ++k)
                windowAcc[(size_t)(outputPos + k)] += window[(size_t)k];

            // 3. Advance output position by fixed step (stride = analysisHop).
            // synthesisPos tracks where we are in the source for the next overlap-add.
            outputPos   += analysisHop;
            synthesisPos += analysisHop;

            // 4. Save overlap tail for next iteration's correlation.
            // The overlap is taken from source at [actualPos + analysisHop, actualPos + frameSize).
            if (overlapLen > 0)
            {
                int tailStart = actualPos + analysisHop;
                int tailCount = juce::jmin(overlapLen, juce::jmax(0, srcLen - tailStart));
                for (int ch = 0; ch < numCh; ++ch)
                {
                    const float* rd = src.getReadPointer(ch);
                    float* po = prevOverlap.getWritePointer(ch);
                    for (int k = 0; k < tailCount; ++k)
                        po[k] = rd[tailStart + k] * window[(size_t)(analysisHop + k)];
                    for (int k = tailCount; k < overlapLen; ++k)
                        po[k] = 0.0f;
                }
                havePrev = true;
            }

            nominalPos += (int)((float)analysisHop / stretchRatio + 0.5f);
        }

        // 4. Normalise by accumulated window weights
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* wr = out.getWritePointer(ch);
            for (int i = 0; i < outLen; ++i)
                if (windowAcc[(size_t)i] > 1.0e-6f) wr[i] /= windowAcc[(size_t)i];
        }

        return out;
    }

    /**
     * Linear-interpolation resampler (additive mix). Used for sample-rate conversion
     * where pitch change alongside speed is acceptable.
     *
     * stretchRatio > 1  →  output is LONGER
     * stretchRatio < 1  →  output is SHORTER
     * source advance per output sample = 1 / stretchRatio
     */
    static void resampleAdd(const juce::AudioBuffer<float>& src,
                            int srcStart,  int srcSamples,
                            juce::AudioBuffer<float>& dest,
                            int destStart, int destSamples,
                            float gainStart = 1.0f,
                            float gainEnd   = 1.0f)
    {
        if (srcSamples <= 0 || destSamples <= 0) return;

        const int srcLen  = src.getNumSamples();
        const int destLen = dest.getNumSamples();
        const int srcCh   = src.getNumChannels();
        const int destCh  = dest.getNumChannels();
        if (srcCh == 0 || srcLen == 0 || destCh == 0) return;

        int dStart = juce::jmax(0, destStart);
        int dEnd   = juce::jmin(destLen, destStart + destSamples);
        if (dStart >= dEnd) return;

        const double advance = (double)srcSamples / (double)destSamples;
        const int    mixCh   = juce::jmin(srcCh, destCh);
        const bool   upmix   = (srcCh == 1 && destCh >= 2);

        for (int ch = 0; ch < mixCh + (upmix ? 1 : 0); ++ch)
        {
            const int readCh   = (upmix && ch == 1) ? 0 : ch;
            const float* rdPtr = src.getReadPointer(readCh);
            float*       wrPtr = dest.getWritePointer(ch);

            double pos = (double)srcStart + (double)(dStart - destStart) * advance;
            for (int i = dStart; i < dEnd; ++i, pos += advance)
            {
                float t = (destSamples > 1)
                          ? (float)(i - destStart) / (float)(destSamples - 1)
                          : 0.0f;
                float g = gainStart + t * (gainEnd - gainStart);

                int   s0   = (int)pos;
                float frac = (float)(pos - (double)s0);
                float a = (s0 >= 0 && s0 < srcLen) ? rdPtr[s0]     : 0.0f;
                float b = (s0 + 1 < srcLen)         ? rdPtr[s0 + 1] : a;

                wrPtr[i] += g * (a + frac * (b - a));
            }
        }
    }
};

} // namespace BlockShuffler
