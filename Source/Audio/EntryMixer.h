#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "ArrangementResolver.h"
#include "TempoStretcher.h"
#include <cmath>

namespace BlockShuffler {

/** Rendered (post-stretch) timeline extent of an entry's lead-in. MUST mirror
 *  the lead-in branch selection in mixEntryToBuffer. Consumers: the mixer's
 *  END-ANCHORED lead placement and PlaybackEngine's culling window. */
inline int64_t renderedLeadInLength(const ResolvedEntry& e)
{
    if (!e.audioBuffer) return 0;
    const int64_t leadInLen = e.startMark;
    if (leadInLen <= 0) return 0;
    if (e.stretchedLeadIn) return (int64_t)e.stretchedLeadIn->getNumSamples();
    if (std::abs(e.leadInStretchRatio - 1.0f) < 0.0001f) return leadInLen;
    return juce::jmax((int64_t)0,
                      (int64_t)(leadInLen * e.leadInStretchRatio + 0.5f));  // FIX M1 guard
}

/** Rendered (post-stretch) timeline extent of an entry's tail — the ACTUAL
 *  overlap the next entry's body fade-in must match. */
inline int64_t renderedTailLength(const ResolvedEntry& e)
{
    if (!e.audioBuffer) return 0;
    const int64_t tailLen = juce::jmax((int64_t)0,
                                       (int64_t)e.audioBuffer->getNumSamples() - e.endMark);
    if (tailLen <= 0) return 0;
    if (e.retainTailTempo || !e.stretchedTail) return tailLen;
    return (int64_t)e.stretchedTail->getNumSamples();
}

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
            destOff    += (int)skip;
            destCount  -= (int)skip;
            if (destCount <= 0 || srcSamples <= 0.0) return;
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
    // JOINFIX (2026-07-19): each source's gain envelope is CONTINUOUS through the
    // join. The lead-in ramps g(k) = (k+1)/L over its RENDERED length L, so its
    // LAST sample sits at gain exactly 1.0 and meets the full-gain body on the
    // same contiguous source audio — no carrier swap at join+0 (OFFGRID C1-C5).
    // It overlaps the PREVIOUS entry's body, which stays at gain 1; the sum may
    // exceed 1.0 by design. Entry 0 has no previous body: full gain, no ramp.
    // resampleAdd applies gainStart at the FIRST and gainEnd at the LAST sample
    // of a full window, so (1/L, 1.0) renders exactly (k+1)/L.
    if (leadInLen > 0)
    {
        auto rampStart = [&](int64_t renderedLen) {
            return (entryIndex == 0 || renderedLen <= 0) ? 1.0f
                                                         : 1.0f / (float)renderedLen;
        };

        // END-ANCHORED (2026-07-19, re-applies 2e93c22): the stretched lead-in
        // ends AT the body join — [bodyStart - sl, bodyStart). Start-anchoring
        // at (bodyStart - unstretched leadInLen) overshot into the body
        // (doubling, ratio > 1) or ended early (gap + click, ratio < 1): XTDIAG.
        // Ratio == 1 degenerates to the identical region.
        if (entry.stretchedLeadIn)
        {
            int64_t sl = (int64_t)entry.stretchedLeadIn->getNumSamples();
            mixBuf(*entry.stretchedLeadIn, bodyStart - sl, bodyStart, 0.0, rampStart(sl), 1.0f);
        }
        else if (std::abs(entry.leadInStretchRatio - 1.0f) < 0.0001f)
        {
            mixBuf(src, leadInStart, bodyStart, 0.0, rampStart(leadInLen), 1.0f);
        }
        else
        {
            int64_t leadInTL = (int64_t)(leadInLen * entry.leadInStretchRatio + 0.5f);
            if (leadInTL > 0)
                mixBuf(src, bodyStart - leadInTL, bodyStart, 0.0, rampStart(leadInTL), 1.0f);
        }
    }

    // ── Body ──────────────────────────────────────────────────────────────────
    // JOINFIX: bodies ALWAYS play at gain exactly 1.0 — never attenuated.
    // Overlapping neighbour lead-ins/tails mix on top at their own ramped gains.
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
