#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "ArrangementResolver.h"
#include "TempoStretcher.h"
#include <cmath>

namespace BlockShuffler {

/**
 * Shared mixing function used by both PlaybackEngine (real-time) and ExportRenderer
 * (offline).  Mixing at a different hardware sample rate from the project is handled
 * by the pToH / hToP conversion factors; pass 1.0 for both when they are the same.
 *
 * @param entry        The resolved entry to mix into the buffer.
 * @param buffer       Output buffer to add into (must already be allocated).
 * @param numSamples   Number of samples in the output buffer window.
 * @param currentHead  Start of the window in hardware samples (0 for export).
 * @param pToH         project SR / hardware SR  (1.0 for export at project SR).
 * @param hToP         hardware SR / project SR  (1.0 for export at project SR).
 * @param entryIndex   Position of this entry in the arrangement (drives lead-in gain).
 */
inline void mixEntryToBuffer(
    const ResolvedEntry& entry,
    juce::AudioBuffer<float>& buffer,
    int numSamples,
    int64_t currentHead,
    double pToH,
    double hToP,
    int entryIndex = -1)
{
    if (!entry.audioBuffer) return;
    const auto& src  = *entry.audioBuffer;
    const int srcCh  = src.getNumChannels();
    const int srcLen = src.getNumSamples();
    const int dstCh  = buffer.getNumChannels();
    if (srcCh == 0 || srcLen == 0 || dstCh == 0) return;

    const int64_t startMark = entry.startMark;
    const int64_t endMark   = entry.endMark;
    const int64_t bodyLen   = endMark - startMark;
    const int64_t leadInLen = startMark;
    const int64_t tailLen   = juce::jmax((int64_t)0, (int64_t)srcLen - endMark);

    // timelinePos = body start; lead-in at [timelinePos - startMark, timelinePos).
    const int64_t bodyStart   = entry.timelinePos;
    const int64_t leadInStart = bodyStart - leadInLen;
    const int64_t bodyEnd     = bodyStart + bodyLen;
    const int64_t blockEnd    = currentHead + (int64_t)numSamples;

    // Mix a region of a source buffer into the output window with optional rate
    // conversion and gain ramp.  All positional arguments are in project samples;
    // hardware-sample conversions are applied internally via pToH / hToP.
    auto mixBuf = [&](const juce::AudioBuffer<float>& s,
                      int64_t regionStart, int64_t regionEnd,
                      double clipOff,
                      float gainStart, float gainEnd)
    {
        const int sCh  = s.getNumChannels();
        const int sLen = s.getNumSamples();
        if (sCh == 0 || sLen == 0) return;

        int64_t regionStartH = (int64_t)((double)regionStart * hToP + 0.5);
        int64_t regionEndH   = (int64_t)((double)regionEnd   * hToP + 0.5);

        int64_t ovStartH = juce::jmax(regionStartH, currentHead);
        int64_t ovEndH   = juce::jmin(regionEndH,   blockEnd);
        if (ovStartH >= ovEndH) return;

        int    destOff    = (int)(ovStartH - currentHead);
        int    destCount  = (int)(ovEndH - ovStartH);

        double pOvStart   = (double)ovStartH * pToH;
        double srcStart   = clipOff + (pOvStart - (double)regionStart);
        double srcSamples = (double)destCount * pToH;

        if (srcStart + srcSamples <= 0.0 || srcStart >= (double)sLen) return;

        if (srcStart < 0.0) {
            double skip = -srcStart;
            srcStart   = 0.0;
            srcSamples -= skip;
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
    // For the first entry there is no prior tail to crossfade with, so the lead-in
    // plays at full gain rather than fading up from 0.
    if (leadInLen > 0)
    {
        const float liGainStart = (entryIndex == 0) ? 1.0f : 0.0f;

        if (entry.stretchedLeadIn)
        {
            int64_t sl = (int64_t)entry.stretchedLeadIn->getNumSamples();
            mixBuf(*entry.stretchedLeadIn, leadInStart, leadInStart + sl, 0.0, liGainStart, 1.0f);
        }
        else if (std::abs(entry.leadInStretchRatio - 1.0f) < 0.0001f)
        {
            mixBuf(src, leadInStart, bodyStart, 0.0, liGainStart, 1.0f);
        }
        else
        {
            // FIX M1: guard against zero-length stretched lead-in
            int64_t leadInTL = (int64_t)(leadInLen * entry.leadInStretchRatio + 0.5f);
            if (leadInTL > 0)
                mixBuf(src, leadInStart, leadInStart + leadInTL, 0.0, liGainStart, 1.0f);
        }
    }

    // ── Body ──────────────────────────────────────────────────────────────────
    if (bodyLen > 0)
        mixBuf(src, bodyStart, bodyEnd, (double)startMark, 1.0f, 1.0f);

    // ── Tail ──────────────────────────────────────────────────────────────────
    if (tailLen > 0)
    {
        // retainTailTempo=true → always play at original speed, never use a
        // pre-stretched buffer (even if one happened to be computed).
        if (entry.retainTailTempo || !entry.stretchedTail)
        {
            mixBuf(src, bodyEnd, bodyEnd + tailLen, (double)endMark, 1.0f, 0.0f);
        }
        else
        {
            int64_t sl = (int64_t)entry.stretchedTail->getNumSamples();
            mixBuf(*entry.stretchedTail, bodyEnd, bodyEnd + sl, 0.0, 1.0f, 0.0f);
        }
    }
}

} // namespace BlockShuffler
