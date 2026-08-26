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
    int64_t      startMark;         // clip's startMark; buffer is the FULL clip snapshot — lead-in [0, startMark), body [startMark, endMark), tail [endMark, len)
    int64_t      endMark;           // clip's endMark; body is [startMark, endMark)
    int64_t      originalStartMark; // same as startMark; kept for compatibility
    bool         retainTailTempo;
    juce::String clipName;
    juce::String clipId;

    int64_t      timelinePos;         // output timeline sample where clip starts
    float        gain;                // mixing gain (1.0 for solo clips, <1.0 for simultaneous layers)
    juce::String blockId;             // id of the Block that produced this entry

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

    // Join-window extents (project samples, RENDERED/post-stretch) — the single
    // source of truth for the ONE complementary crossfade window at each join
    // (JOINFIX2 2026-07-20; restores the section-5b coupling removed in 36fd316).
    // prevTailLen   = previous entry's rendered tail (overlaps this body's start)
    // nextLeadInLen = next entry's rendered lead-in (overlaps this body's end)
    int64_t prevTailLen   = 0;
    int64_t nextLeadInLen = 0;

    /** RAWGAIN: copied from Project::unityGainMode at resolve time. Carried on
     *  the entry because mixEntryToBuffer receives only the entry — this is what
     *  makes playback AND export honour the flag through the one mixer path.
     *  Default false so any entry built outside the resolver keeps the fade law. */
    bool unityGainMode = false;
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

    /** Resolves the project's probabilistic model into a concrete arrangement.
     *
     *  PIN (play-from-here, 2026-08-22): when @p forceInclude is non-null, that
     *  block is GUARANTEED to appear in the result — its playChance gate is
     *  bypassed, it is pre-picked into its stack's selection (REPLACING one of
     *  the sampled members, never adding to them), and song enders in EARLIER
     *  slots do not truncate the arrangement before it is reached. Enders at or
     *  after the pinned block truncate normally.
     *
     *  The pin cannot rescue a block with no clips or with every clip at 0%
     *  weight — the UI disables "Play from Here" for those (BlockComponent).
     *
     *  With @p forceInclude left at nullptr the result is bit-identical to the
     *  pre-pin resolver, RNG draw order included (locked by T62's golden dump
     *  hash over 100 seeds). */
    ResolvedArrangement resolve(const Project& project, juce::Random& rng,
                                const Block* forceInclude = nullptr) const;
    static Clip* pickClip(const Block& block, juce::Random& rng);
};

} // namespace BlockShuffler
