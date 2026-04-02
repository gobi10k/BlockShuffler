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
 *   Uses atomic pointer swap for the arrangement to avoid locks on audio thread.
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

    void mixEntryIntoBuffer(juce::AudioBuffer<float>& buffer,
                            int numSamples,
                            const ResolvedEntry& entry,
                            int64_t currentHead,
                            double pToH,
                            double hToP) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaybackEngine)
};

} // namespace BlockShuffler
