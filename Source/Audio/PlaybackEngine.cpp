#include "PlaybackEngine.h"
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
        activeArrangement = std::move(next);
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

    for (const auto& entry : current->entries) {
        const int64_t bodyLen   = entry.clip->endMark - entry.clip->startMark;
        const int64_t leadInLen = entry.clip->startMark;
        const int64_t tailLen   = juce::jmax((int64_t)0,
                                             (int64_t)entry.clip->audioBuffer.getNumSamples()
                                             - entry.clip->endMark);

        // Full range including stretched lead-in and tail (in project samples)
        const int64_t leadInTL  = entry.stretchedLeadIn
                                  ? (int64_t)entry.stretchedLeadIn->getNumSamples()
                                  : (int64_t)(leadInLen * entry.leadInStretchRatio + 0.5f);
        const int64_t tailTL    = entry.stretchedTail
                                  ? (int64_t)entry.stretchedTail->getNumSamples()
                                  : (int64_t)(tailLen   * entry.tailStretchRatio   + 0.5f);

        // Convert project-space bounds to hardware-space bounds
        int64_t fullStartH = (int64_t)((double)(entry.timelinePos - leadInTL) * hToP + 0.5);
        int64_t fullEndH   = (int64_t)((double)(entry.timelinePos + bodyLen + tailTL) * hToP + 0.5);

        if (fullEndH <= head || fullStartH >= head + (int64_t)numSamples) continue;

        mixEntryIntoBuffer(buffer, numSamples, entry, head, pToH, hToP);
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
                                         double hToP) const
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

    // General region mixer with resampling for pitch correction.
    // regionStart/regionEnd are in project-sample space.
    auto mixBuf = [&](const juce::AudioBuffer<float>& s,
                      int64_t regionStart, int64_t regionEnd,
                      double clipOff,
                      float gainStart, float gainEnd)
    {
        const int sCh  = s.getNumChannels();
        const int sLen = s.getNumSamples();
        if (sCh == 0 || sLen == 0) return;

        // Convert project region to hardware-sample region
        int64_t regionStartH = (int64_t)((double)regionStart * hToP + 0.5);
        int64_t regionEndH   = (int64_t)((double)regionEnd   * hToP + 0.5);

        int64_t ovStartH = juce::jmax(regionStartH, currentHead);
        int64_t ovEndH   = juce::jmin(regionEndH,   blockEnd);
        if (ovStartH >= ovEndH) return;

        int    destOff   = (int)(ovStartH - currentHead);
        int    destCount = (int)(ovEndH - ovStartH);

        // Find corresponding project-sample range for source reading
        double pOvStart  = (double)ovStartH * pToH;
        double srcStart  = clipOff + (pOvStart - (double)regionStart);
        double srcSamples = (double)destCount * pToH;

        if (srcStart + srcSamples <= 0.0 || srcStart >= (double)sLen) return;

        if (srcStart < 0.0) {
            double skip = -srcStart;
            srcStart = 0.0;
            srcSamples -= skip;
            // (destOff and destCount would also need adjustment if we wanted perfect precision here)
        }

        const int64_t regLen = regionEnd - regionStart;
        float gs = gainStart, ge = gainEnd;
        if (regLen > 1) {
            float t0 = (float)(pOvStart - (double)regionStart) / (float)regLen;
            float t1 = (float)((double)pOvStart + srcSamples - (double)regionStart) / (float)regLen;
            gs = gainStart + t0 * (gainEnd - gainStart);
            ge = gainStart + t1 * (gainEnd - gainStart);
        }
        gs *= entry.gain;
        ge *= entry.gain;

        TempoStretcher::resampleAdd(s, srcStart, srcSamples, buffer, destOff, destCount, gs, ge);
    };

    // ── Lead-in ────────────────────────────────────────────────────────────────
    if (leadInLen > 0)
    {
        if (entry.stretchedLeadIn)
        {
            // Pre-stretched buffer: timeline [bodyStart-stretchedLen, bodyStart)
            int64_t sl = (int64_t)entry.stretchedLeadIn->getNumSamples();
            mixBuf(*entry.stretchedLeadIn, bodyStart - sl, bodyStart, 0.0, 0.0f, 1.0f);
        }
        else if (std::abs(entry.leadInStretchRatio - 1.0f) < 0.0001f)
        {
            // No stretching needed (WSOLA skipped)
            mixBuf(src, bodyStart - leadInLen, bodyStart, 0.0, 0.0f, 1.0f);
        }
        else
        {
            // Fallback linear-interp (WSOLA failed or was bypassed)
            int64_t leadInTL = (int64_t)(leadInLen * entry.leadInStretchRatio + 0.5f);
            mixBuf(src, bodyStart - leadInTL, bodyStart, 0.0, 0.0f, 1.0f);
        }
    }

    // ── Body ──────────────────────────────────────────────────────────────────
    if (bodyLen > 0)
        mixBuf(src, bodyStart, bodyEnd, (double)startMark, 1.0f, 1.0f);

    // ── Tail ──────────────────────────────────────────────────────────────────
    if (tailLen > 0)
    {
        // retainTailTempo=true → always use original clip audio at 1:1, never
        // use a pre-stretched buffer (even if one happened to exist).
        if (entry.clip->retainTailTempo || !entry.stretchedTail)
        {
            mixBuf(src, bodyEnd, bodyEnd + tailLen, (double)endMark, 1.0f, 0.0f);
        }
        else
        {
            // Pre-stretched buffer (retainTailTempo=false, tempos differ)
            int64_t sl = (int64_t)entry.stretchedTail->getNumSamples();
            mixBuf(*entry.stretchedTail, bodyEnd, bodyEnd + sl, 0.0, 1.0f, 0.0f);
        }
    }
}

} // namespace BlockShuffler
