#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace BlockShuffler {

/**
 * Stateless per-sample soft-clip applied ONCE to the FINAL summed mix, by both
 * PlaybackEngine (per output block) and ExportRenderer (whole buffer before the
 * writer). Because it is a pure per-sample transfer curve — no attack/release/
 * lookahead state — block-by-block playback and single-pass export remain
 * bit-identical, and it is audio-thread safe (no allocation, locks, or I/O).
 *
 * Transfer curve:  identity for |x| <= T (sub-threshold audio passes
 * BIT-IDENTICAL), and for |x| > T
 *
 *     f(x) = sign(x) * ( T + (1-T) * tanh( (|x|-T) / (1-T) ) )
 *
 * which is C1-continuous at the knee (value T, slope 1) and asymptotes to
 * +/-1.0, so no output sample can exceed full scale for ANY input level.
 *
 * NEVER apply this inside mixEntryToBuffer — limiting a partial sum is wrong;
 * only the final sum of all entries may be shaped.
 */
struct SoftLimiter
{
    /** Knee threshold. Above the hottest normal program level (manual-project
     *  tones peak 0.6; crossfade sums overshoot into 1.05-2.14), below FS. */
    static constexpr float threshold = 0.9f;

    static inline float shape(float x) noexcept
    {
        const float a = std::abs(x);
        if (a <= threshold) return x;                 // bit-identical below T
        constexpr float knee = 1.0f - threshold;
        const float y = threshold + knee * std::tanh((a - threshold) / knee);
        return x > 0.0f ? y : -y;
    }

    /** Shape the first numSamples of every channel in place. */
    static void process(juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        const int n = juce::jmin(numSamples, buffer.getNumSamples());
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* p = buffer.getWritePointer(ch);
            for (int i = 0; i < n; ++i)
                p[i] = shape(p[i]);
        }
    }
};

} // namespace BlockShuffler
