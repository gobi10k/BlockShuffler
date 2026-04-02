#include "PlaybackEngine.h"
#include "TempoStretcher.h"

namespace BlockShuffler {

PlaybackEngine::PlaybackEngine() {}

void PlaybackEngine::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    juce::ScopedLock sl(lock);
    outputSampleRate = sampleRate;
}

void PlaybackEngine::releaseResources() {
    stop();
}

void PlaybackEngine::play(ResolvedArrangement newArrangement) {
    juce::ScopedLock sl(lock);
    arrangement = std::move(newArrangement);
    playheadSamples.store(0);
    playing.store(true);
}

void PlaybackEngine::stop() {
    playing.store(false);
}

void PlaybackEngine::rewind() {
    juce::ScopedLock sl(lock);
    playheadSamples.store(0);
}

double PlaybackEngine::getPlayheadSeconds() const {
    if (outputSampleRate <= 0.0) return 0.0;
    return (double)playheadSamples.load() / outputSampleRate;
}

double PlaybackEngine::getTotalSeconds() const {
    juce::ScopedLock sl(lock);
    if (outputSampleRate <= 0.0 || arrangement.isEmpty()) return 0.0;
    return (double)arrangement.totalDurationSamples / outputSampleRate;
}

void PlaybackEngine::getNextAudioBlock(juce::AudioBuffer<float>& buffer, int numSamples) {
    buffer.clear();

    if (!playing.load()) return;

    juce::ScopedLock sl(lock);

    if (arrangement.isEmpty()) return;

    int64_t head = playheadSamples.load();  // read inside lock for consistency with play()

    for (const auto& entry : arrangement.entries) {
        const int64_t bodyLen   = entry.clip->endMark - entry.clip->startMark;
        const int64_t leadInLen = entry.clip->startMark;
        const int64_t tailLen   = juce::jmax((int64_t)0,
                                             (int64_t)entry.clip->audioBuffer.getNumSamples()
                                             - entry.clip->endMark);

        // Full range including stretched lead-in and tail
        const int64_t leadInTL  = entry.stretchedLeadIn
                                  ? (int64_t)entry.stretchedLeadIn->getNumSamples()
                                  : (int64_t)(leadInLen * entry.leadInStretchRatio + 0.5f);
        const int64_t tailTL    = entry.stretchedTail
                                  ? (int64_t)entry.stretchedTail->getNumSamples()
                                  : (int64_t)(tailLen   * entry.tailStretchRatio   + 0.5f);
        const int64_t fullStart = entry.timelinePos - leadInTL;
        const int64_t fullEnd   = entry.timelinePos + bodyLen + tailTL;

        if (fullEnd <= head || fullStart >= head + (int64_t)numSamples) continue;

        mixEntryIntoBuffer(buffer, numSamples, entry, head);
    }

    head += numSamples;
    playheadSamples.store(head);

    // Stop at end of arrangement
    if (head >= arrangement.totalDurationSamples) {
        playing.store(false);
        playheadSamples.store(arrangement.totalDurationSamples);
    }
}

void PlaybackEngine::mixEntryIntoBuffer(juce::AudioBuffer<float>& buffer,
                                         int numSamples,
                                         const ResolvedEntry& entry,
                                         int64_t currentHead) const
{
    const auto& src  = entry.clip->audioBuffer;
    const int srcCh  = src.getNumChannels();
    const int srcLen = src.getNumSamples();
    const int dstCh  = buffer.getNumChannels();
    if (srcCh == 0 || srcLen == 0 || dstCh == 0) return;

    const int64_t startMark = entry.clip->startMark;
    const int64_t endMark   = entry.clip->endMark;
    const int64_t bodyLen   = endMark - startMark;
    const int64_t leadInLen = startMark;
    const int64_t tailLen   = juce::jmax((int64_t)0, (int64_t)srcLen - endMark);

    const int64_t bodyStart = entry.timelinePos;
    const int64_t bodyEnd   = bodyStart + bodyLen;
    const int64_t blockEnd  = currentHead + (int64_t)numSamples;
    const int mixCh = juce::jmin(srcCh, dstCh);

    // General 1:1 region mixer for any source buffer with gain ramp.
    // regionStart/regionEnd are timeline positions; clipOff is the source sample index
    // at regionStart.
    auto mixBuf = [&](const juce::AudioBuffer<float>& s,
                      int64_t regionStart, int64_t regionEnd,
                      int64_t clipOff,
                      float gainStart, float gainEnd)
    {
        const int sCh  = s.getNumChannels();
        const int sLen = s.getNumSamples();
        const int mCh  = juce::jmin(sCh, dstCh);
        if (sCh == 0 || sLen == 0) return;

        int64_t ovStart = juce::jmax(regionStart, currentHead);
        int64_t ovEnd   = juce::jmin(regionEnd, blockEnd);
        if (ovStart >= ovEnd) return;

        int     destOff = (int)(ovStart - currentHead);
        int64_t srcOff  = clipOff + (ovStart - regionStart);
        int     count   = (int)(ovEnd - ovStart);

        if (srcOff < 0) { int skip = (int)(-srcOff); destOff += skip; count -= skip; srcOff = 0; }
        if (srcOff >= (int64_t)sLen) return;
        count = juce::jmin(count, sLen - (int)srcOff);
        if (count <= 0) return;

        const int64_t regLen = regionEnd - regionStart;
        float gs = gainStart, ge = gainEnd;
        if (regLen > 1) {
            float t0 = (float)(ovStart - regionStart) / (float)regLen;
            float t1 = (float)(ovEnd   - regionStart) / (float)regLen;
            gs = gainStart + t0 * (gainEnd - gainStart);
            ge = gainStart + t1 * (gainEnd - gainStart);
        }
        gs *= entry.gain;
        ge *= entry.gain;

        for (int ch = 0; ch < mCh; ++ch)
            buffer.addFromWithRamp(ch, destOff, s.getReadPointer(ch) + srcOff, count, gs, ge);
        if (sCh == 1 && dstCh >= 2)
            buffer.addFromWithRamp(1, destOff, s.getReadPointer(0) + srcOff, count, gs, ge);
    };

    // ── Lead-in ────────────────────────────────────────────────────────────────
    if (leadInLen > 0)
    {
        if (entry.stretchedLeadIn)
        {
            // Pre-stretched buffer: mix at 1:1, timeline [bodyStart-stretchedLen, bodyStart)
            int64_t sl = (int64_t)entry.stretchedLeadIn->getNumSamples();
            mixBuf(*entry.stretchedLeadIn, bodyStart - sl, bodyStart, 0, 0.0f, 1.0f);
        }
        else if (std::abs(entry.leadInStretchRatio - 1.0f) < 0.0001f)
        {
            // No stretching needed
            mixBuf(src, bodyStart - leadInLen, bodyStart, 0, 0.0f, 1.0f);
        }
        else
        {
            // Fallback linear-interp (should not normally be reached since WSOLA pre-computes)
            int64_t leadInTL = (int64_t)(leadInLen * entry.leadInStretchRatio + 0.5f);
            int64_t lStart   = bodyStart - leadInTL;
            int64_t ovStart  = juce::jmax(lStart, currentHead);
            int64_t ovEnd    = juce::jmin(bodyStart, blockEnd);
            if (ovStart < ovEnd)
            {
                double srcAdv = (double)leadInLen / (double)leadInTL;
                int destOff   = (int)(ovStart - currentHead);
                int destCount = (int)(ovEnd - ovStart);
                float gs = (leadInTL > 1) ? (float)(ovStart - lStart) / (float)(leadInTL - 1) * entry.gain : 0.0f;
                float ge = (leadInTL > 1) ? (float)(ovEnd   - lStart) / (float)(leadInTL - 1) * entry.gain : entry.gain;
                int srcSliceOff = (int)((double)(ovStart - lStart) * srcAdv);
                int srcSliceCnt = (int)((double)destCount * srcAdv + 1.5);
                srcSliceCnt = juce::jmin(srcSliceCnt, (int)leadInLen - srcSliceOff);
                TempoStretcher::resampleAdd(src, srcSliceOff, srcSliceCnt, buffer, destOff, destCount, gs, ge);
            }
        }
    }

    // ── Body (always 1:1) ─────────────────────────────────────────────────────
    if (bodyLen > 0)
        mixBuf(src, bodyStart, bodyEnd, startMark, 1.0f, 1.0f);

    // ── Tail ──────────────────────────────────────────────────────────────────
    if (tailLen > 0)
    {
        // retainTailTempo=true → always use original clip audio at 1:1, never
        // use a pre-stretched buffer (even if one happened to exist).
        if (entry.clip->retainTailTempo || !entry.stretchedTail)
        {
            mixBuf(src, bodyEnd, bodyEnd + tailLen, endMark, 1.0f, 0.0f);
        }
        else
        {
            // Pre-stretched buffer (retainTailTempo=false, tempos differ)
            int64_t sl = (int64_t)entry.stretchedTail->getNumSamples();
            mixBuf(*entry.stretchedTail, bodyEnd, bodyEnd + sl, 0, 1.0f, 0.0f);
        }
    }
}

} // namespace BlockShuffler
