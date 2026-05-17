#include "PlaybackEngine.h"
#include "EntryMixer.h"
#include "TempoStretcher.h"

namespace BlockShuffler {

PlaybackEngine::PlaybackEngine() {}

void PlaybackEngine::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    outputSampleRate = sampleRate;
}

void PlaybackEngine::releaseResources() {
    stop();
}

void PlaybackEngine::play(ResolvedArrangement newArr) {
    auto next = std::make_shared<const ResolvedArrangement>(std::move(newArr));
    {
        juce::ScopedLock sl(arrangementLock);
        activeArrangement = next;
    }
    playheadSamples.store(0);
    playing.store(true);
}

void PlaybackEngine::stop() {
    playing.store(false);
}

void PlaybackEngine::rewind() {
    playheadSamples.store(0);
}

void PlaybackEngine::seekTo(int64_t projectSample) {
    std::shared_ptr<const ResolvedArrangement> current;
    {
        juce::ScopedLock sl(arrangementLock);
        current = activeArrangement;
    }
    if (!current || current->sampleRate <= 0.0 || outputSampleRate <= 0.0) {
        playheadSamples.store(0);
        return;
    }
    // FIX M3: clamp to valid range before converting to hardware samples
    projectSample = juce::jlimit((int64_t)0, current->totalDurationSamples, projectSample);
    int64_t hw = (int64_t)((double)projectSample * outputSampleRate / current->sampleRate + 0.5);
    playheadSamples.store(hw);
}

double PlaybackEngine::getPlayheadSeconds() const {
    if (outputSampleRate <= 0.0) return 0.0;
    return (double)playheadSamples.load() / outputSampleRate;
}

double PlaybackEngine::getTotalSeconds() const {
    std::shared_ptr<const ResolvedArrangement> current;
    {
        juce::ScopedLock sl(arrangementLock);
        current = activeArrangement;
    }
    if (!current || current->isEmpty() || current->sampleRate <= 0.0) return 0.0;
    return (double)current->totalDurationSamples / current->sampleRate;
}

void PlaybackEngine::getNextAudioBlock(juce::AudioBuffer<float>& buffer, int numSamples) {
    buffer.clear();

    if (!playing.load()) return;

    std::shared_ptr<const ResolvedArrangement> current;
    {
        // Try-lock on audio thread to avoid blocking.
        // If we can't get the lock, we skip this block (silence).
        if (!arrangementLock.tryEnter()) return;
        current = activeArrangement;
        arrangementLock.exit();
    }

    if (!current || current->isEmpty()) return;

    int64_t head = playheadSamples.load();
    const double pToH = (current && current->sampleRate > 0.0 && outputSampleRate > 0.0)
                        ? current->sampleRate / outputSampleRate
                        : 1.0;
    const double hToP = 1.0 / pToH;

    for (int entryIndex = 0; entryIndex < current->entries.size(); ++entryIndex) {
        const auto& entry = current->entries.getReference(entryIndex);
        const int64_t tailLen   = (entry.audioBuffer) ? juce::jmax((int64_t)0,
                                             (int64_t)entry.audioBuffer->getNumSamples()
                                             - entry.endMark) : 0;

        // Full range: lead-in starts at timelinePos, body at timelinePos+startMark,
        // tail at timelinePos+endMark, tail ends at timelinePos+endMark+tailTL.
        const int64_t tailTL    = entry.stretchedTail
                                  ? (int64_t)entry.stretchedTail->getNumSamples()
                                  : (int64_t)(tailLen   * entry.tailStretchRatio   + 0.5f);

        // timelinePos = body start; lead-in at [timelinePos - startMark, timelinePos).
        // Convert project-space bounds to hardware-space bounds.
        int64_t fullStartH = (int64_t)((double)(entry.timelinePos - entry.startMark) * hToP + 0.5);
        int64_t fullEndH   = (int64_t)((double)(entry.timelinePos + (entry.endMark - entry.startMark) + tailTL) * hToP + 0.5);

        if (fullEndH <= head || fullStartH >= head + (int64_t)numSamples) continue;

        mixEntryIntoBuffer(buffer, numSamples, entry, head, pToH, hToP, entryIndex);
    }

    head += numSamples;
    playheadSamples.store(head);

    // Stop at end of arrangement (convert total duration to hardware samples)
    int64_t totalH = (int64_t)((double)current->totalDurationSamples * hToP + 0.5);
    if (head >= totalH) {
        playing.store(false);
        playheadSamples.store(totalH);
    }
}

void PlaybackEngine::mixEntryIntoBuffer(juce::AudioBuffer<float>& buffer,
                                         int numSamples,
                                         const ResolvedEntry& entry,
                                         int64_t currentHead,
                                         double pToH,
                                         double hToP,
                                         int entryIndex) const
{
    // FIX H1: delegate to shared EntryMixer so playback and export use identical logic
    mixEntryToBuffer(entry, buffer, numSamples, currentHead, pToH, hToP, entryIndex);
}

} // namespace BlockShuffler
