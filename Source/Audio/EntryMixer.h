#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "ArrangementResolver.h"
#include "TempoStretcher.h"
#include <cmath>

namespace BlockShuffler {

/** Rendered (post-stretch) timeline extent of an entry's lead-in. MUST mirror
 *  the lead-in branch selection in mixEntryToBuffer. Consumers: the mixer's
 *  END-ANCHORED lead placement, PlaybackEngine's culling window, and the
 *  resolver's cross-fade length sync (section 5b). */
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
    // For the first entry there is no prior tail to crossfade with, so the lead-in
    // plays at full gain rather than fading up from 0.
    if (leadInLen > 0)
    {
        const float liGainStart = (entryIndex == 0) ? 1.0f : 0.0f;

        // END-ANCHORED (2026-07-19, re-applies 2e93c22): the stretched lead-in
        // ends AT the body join — [bodyStart - sl, bodyStart). Start-anchoring
        // at (bodyStart - unstretched leadInLen) overshot into the body
        // (doubling, ratio > 1) or ended early (gap + click, ratio < 1): XTDIAG.
        // Ratio == 1 degenerates to the identical region.
        if (entry.stretchedLeadIn)
        {
            int64_t sl = (int64_t)entry.stretchedLeadIn->getNumSamples();
            mixBuf(*entry.stretchedLeadIn, bodyStart - sl, bodyStart, 0.0, liGainStart, 1.0f);
        }
        else if (std::abs(entry.leadInStretchRatio - 1.0f) < 0.0001f)
        {
            mixBuf(src, leadInStart, bodyStart, 0.0, liGainStart, 1.0f);
        }
        else
        {
            int64_t leadInTL = (int64_t)(leadInLen * entry.leadInStretchRatio + 0.5f);
            if (leadInTL > 0)
                mixBuf(src, bodyStart - leadInTL, bodyStart, 0.0, liGainStart, 1.0f);
        }
    }

    // ── Body ──────────────────────────────────────────────────────────────────
    // Apply complementary cross‑fades with adjacent entries.
    if (bodyLen > 0)
    {
        int64_t fadeInLen  = entry.prevTailLen;    // overlap with previous tail
        int64_t fadeOutLen = entry.nextLeadInLen;  // overlap with next lead‑in

        // Clamp to body length
        fadeInLen  = juce::jmin(fadeInLen, bodyLen);
        fadeOutLen = juce::jmin(fadeOutLen, bodyLen);

        // If the fade regions overlap, scale them proportionally so they fit.
        if (fadeInLen + fadeOutLen > bodyLen) {
            float scale = (float)bodyLen / (float)(fadeInLen + fadeOutLen);
            fadeInLen  = (int64_t)(fadeInLen * scale);
            fadeOutLen = (int64_t)(fadeOutLen * scale);
            // Re‑compute to ensure sum does not exceed bodyLen
            if (fadeInLen + fadeOutLen > bodyLen) {
                // If rounding caused an overflow, trim the longer one.
                if (fadeInLen > fadeOutLen) fadeInLen = bodyLen - fadeOutLen;
                else fadeOutLen = bodyLen - fadeInLen;
            }
        }

        int64_t midStart = bodyStart + fadeInLen;
        int64_t midEnd   = bodyEnd - fadeOutLen;

        // Constant gain segment
        if (midStart < midEnd) {
            double clipOff = (double)startMark + (double)(midStart - bodyStart);
            mixBuf(src, midStart, midEnd, clipOff, 1.0f, 1.0f);
        }

        // Fade‑in from 0 → 1 (overlap with previous tail)
        if (fadeInLen > 0) {
            int64_t fadeInEnd = bodyStart + fadeInLen;
            double clipOff = (double)startMark;
            mixBuf(src, bodyStart, fadeInEnd, clipOff, 0.0f, 1.0f);
        }

        // Fade‑out from 1 → 0 (overlap with next lead‑in)
        if (fadeOutLen > 0) {
            int64_t fadeOutStart = bodyEnd - fadeOutLen;
            double clipOff = (double)startMark + (double)(fadeOutStart - bodyStart);
            mixBuf(src, fadeOutStart, bodyEnd, clipOff, 1.0f, 0.0f);
        }
    }

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
