#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "ArrangementResolver.h"

namespace BlockShuffler {

/**
 * Renders a ResolvedArrangement to a single audio file (WAV or FLAC).
 * Runs synchronously — call from a background thread for long arrangements.
 */
class ExportRenderer {
public:
    using ProgressFn = std::function<void(float)>;  // 0.0 – 1.0

    ExportRenderer() = default;

    /**
     * Render arrangement to file.
     * @param format   Pass a WavAudioFormat or FlacAudioFormat instance.
     * @param bitDepth 16 or 24.
     * @param progress Optional progress callback (called from the calling thread).
     * @return true on success.
     */
    bool renderToFile(const ResolvedArrangement& arrangement,
                      const juce::File& outputFile,
                      juce::AudioFormat& format,
                      int bitDepth = 24,
                      ProgressFn progress = nullptr);

    /**
     * Render arrangement to a .bsf bundle (ZIP containing per-clip FLACs + manifest.json).
     * The mobile player can unzip and play clips sequentially per the manifest.
     *
     * @param projectSnapshot  Optional: full project JSON (from Project::toJSON()).
     *                         If provided, a model.json is added to the bundle containing
     *                         the complete probabilistic model so a future mobile player
     *                         can re-randomise on-device.
     */
    bool renderToBsf(const ResolvedArrangement& arrangement,
                     const juce::File& outputFile,
                     int bitDepth = 24,
                     ProgressFn progress = nullptr,
                     const juce::var& projectSnapshot = {});

private:
    static bool writeClipFlac(const Clip& clip, const juce::File& dest, int bitDepth, double sampleRate);
    static void mixEntry(juce::AudioBuffer<float>& dest,
                         const ResolvedEntry& entry);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportRenderer)
};

} // namespace BlockShuffler
