#include "PlaybackEngine.h"
#include "EntryMixer.h"

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
        // If we can't get the lock, skip this block (silence).
        if (!arrangementLock.tryEnter()) return;
        current = activeArrangement;
        arrangementLock.exit();
    }

    if (!current || current->isEmpty()) return;

    const int64_t head = playheadSamples.load();
    const double pToH = (current->sampleRate > 0.0 && outputSampleRate > 0.0)
                        ? current->sampleRate / outputSampleRate
                        : 1.0;
    const double hToP = 1.0 / pToH;

    for (int i = 0; i < current->entries.size(); ++i)
    {
        const auto& entry = current->entries.getReference(i);
        const int64_t bodyLen  = entry.endMark - entry.startMark;
        const int64_t leadInLen = entry.startMark;
        const int64_t tailLen   = entry.audioBuffer
                                  ? juce::jmax((int64_t)0,
                                               (int64_t)entry.audioBuffer->getNumSamples()
                                               - entry.endMark)
                                  : 0;

        const int64_t leadInTL = entry.stretchedLeadIn
                                 ? (int64_t)entry.stretchedLeadIn->getNumSamples()
                                 : (int64_t)std::llround((double)leadInLen * entry.leadInStretchRatio);
        const int64_t tailTL   = entry.stretchedTail
                                 ? (int64_t)entry.stretchedTail->getNumSamples()
                                 : (int64_t)std::llround((double)tailLen * entry.tailStretchRatio);

        // Full hardware-space window for this entry (lead-in through tail)
        const int64_t fullStartH = (int64_t)((double)(entry.timelinePos - (i > 0 ? leadInTL : 0)) * hToP + 0.5);
        const int64_t fullEndH   = (int64_t)((double)(entry.timelinePos + bodyLen + tailTL) * hToP + 0.5);

        if (fullEndH <= head || fullStartH >= head + (int64_t)numSamples) continue;

        mixEntryIntoBuffer(buffer, numSamples, entry, head, pToH, hToP, i);
    }

    const int64_t newHead = head + numSamples;
    playheadSamples.store(newHead);

    // Stop at end of arrangement (convert total duration to hardware samples)
    const int64_t totalH = (int64_t)((double)current->totalDurationSamples * hToP + 0.5);
    if (newHead >= totalH) {
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
    mixEntryToBuffer(entry, buffer, currentHead, numSamples, entryIndex, pToH, hToP);
}

} // namespace BlockShuffler
