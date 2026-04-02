#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include "../Model/Project.h"

namespace BlockShuffler {

/** A single clip event in the resolved timeline.
 *  Stored as a self-contained snapshot of audio data and metadata to avoid
 *  dangling pointers if model objects are deleted during playback. */
struct ResolvedEntry {
    std::shared_ptr<juce::AudioBuffer<float>> audioBuffer;
    int64_t      startMark;
    int64_t      endMark;
    bool         retainTailTempo;
    juce::String clipName;
    juce::String clipId;

    int64_t      timelinePos;         // output timeline sample where clip->startMark lands
    float        gain;                // mixing gain (1.0 for solo clips, <1.0 for simultaneous layers)
    juce::String blockId;             // id of the Block that produced this entry

    bool         isOverlay = false;  // true = overlapping block, layers on top of primary entry

    // Tempo stretching for lead-in / tail transitions.
    // ratio = clip.tempo / adjacentClip.tempo
    //   > 1.0  →  output is LONGER  (slowed down, pitch unchanged)
    //   < 1.0  →  output is SHORTER (sped up,    pitch unchanged)
    //   1.0    →  no stretching (default, and when retainLeadInTempo/retainTailTempo is set)
    float leadInStretchRatio = 1.0f;  // default: no stretch; overridden by post-processing
    float tailStretchRatio   = 1.0f;  // default: no stretch; overridden by post-processing

    // Pre-computed pitch-preserving stretched buffers (null if no stretching needed).
    std::shared_ptr<juce::AudioBuffer<float>> stretchedLeadIn = {};
    std::shared_ptr<juce::AudioBuffer<float>> stretchedTail   = {};
};

/** Concrete arrangement produced by resolving probabilities. */
struct ResolvedArrangement {
    juce::Array<ResolvedEntry> entries;
    int64_t totalDurationSamples = 0;
    double  sampleRate           = 48000.0;

    bool isEmpty() const { return entries.isEmpty(); }
};

/**
 * Resolves a Project's probabilistic model into a concrete ResolvedArrangement.
 * Call on the UI thread. Pass the result to PlaybackEngine::play().
 */
class ArrangementResolver {
public:
    ArrangementResolver() = default;
    ResolvedArrangement resolve(const Project& project, juce::Random& rng) const;

private:
    static Clip* pickClip(const Block& block, juce::Random& rng);
};

} // namespace BlockShuffler
