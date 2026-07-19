#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include "ArrangementResolver.h"

namespace BlockShuffler {

/**
 * Mixes a ResolvedArrangement into an audio output stream.
 *
 * Thread safety:
 *   play() / stop() / rewind() are called on the UI thread.
 *   getNextAudioBlock() is called on the audio thread.
 *   Uses CriticalSection with tryEnter on the audio thread — on lock contention
 *   the audio block is silenced rather than blocking the audio callback.
 *   Arrangement swaps are infrequent relative to the audio callback rate, so
 *   contention is rare in practice.
 */
class PlaybackEngine {
public:
    PlaybackEngine();
    ~PlaybackEngine() = default;

    // Called by PluginProcessor
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();
    void getNextAudioBlock(juce::AudioBuffer<float>& buffer, int numSamples);

    // UI-thread transport controls
    void play(ResolvedArrangement arrangement);
    void stop();
    void rewind();
    /** Seek to a position expressed in project samples (converts to hardware samples internally). */
    void seekTo(int64_t projectSample);

    // Query (approximate, may be off by one block)
    bool   isPlaying()         const { return playing.load(); }
    double getPlayheadSeconds() const;
    double getTotalSeconds()    const;

private:
    juce::CriticalSection arrangementLock;
    std::shared_ptr<const ResolvedArrangement> activeArrangement;

    std::atomic<bool>    playing    { false };
    std::atomic<int64_t> playheadSamples { 0 };

    double outputSampleRate = 48000.0;

    // Removed the old mixEntryIntoBuffer declaration - we now use the global function

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaybackEngine)
};

} // namespace BlockShuffler
