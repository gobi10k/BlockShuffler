#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "ArrangementResolver.h"
#include "TempoStretcher.h"
#include <cmath>

namespace BlockShuffler {

/** Rendered (post-stretch) timeline extent of an entry's lead-in. MUST mirror
 *  the lead-in branch selection in mixEntryToBuffer. Consumers: the mixer's
 *  END-ANCHORED lead placement, PlaybackEngine's culling window, and the
 *  resolver's join-window sync (section 5b). */
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

    // ── JOINFIX2 join windows (2026-07-20) ─────────────────────────────────────
    // ONE continuous complementary crossfade per join replaces the bodies-at-
    // full-gain topology (36fd316): that cured the carrier-swap seam but summed
    // two full-gain sources at every join (C6-hot 1.879 FS — hard clip at any
    // DAC or integer export). Per join at B.timelinePos: window [join−L, join+T),
    // W = L+T, L = B's RENDERED lead-in, T = A's RENDERED tail (resolver
    // section 5b: nextLeadInLen / prevTailLen — the single source of truth).
    //   A (body-end → tail, contiguous source): gA(p) = (W−1−p)/(W−1)
    //   B (lead-in → body-start, contiguous):   gB(p) = p/(W−1)
    // gA + gB ≡ 1 ⇒ |out| ≤ 1 for sources ≤ 1. LINEAR only — equal-power sums
    // to 1.41 and forfeits the bound. Endpoint gains are evaluated from the
    // window line at INCLUSIVE sample positions (resampleAdd applies gainStart
    // at the first and gainEnd at the LAST sample), so split regions and
    // 512-chunk edges stay on one line; complementary A/B regions share bounds,
    // and mixBuf's interpolation is linear in the endpoints, so their per-sample
    // gains sum to exactly 1. Degenerate L or T: same formulas, W = remainder.
    const int64_t joinLeadIn = renderedLeadInLength(entry);   // L of this entry's B-side window
    const int64_t joinTail   = renderedTailLength(entry);     // T of this entry's A-side window
    const int64_t prevT      = (entryIndex == 0) ? 0 : entry.prevTailLen;
    const int64_t Wb   = joinLeadIn + prevT;                  // window this entry ENTERS through
    const int64_t Wa   = entry.nextLeadInLen + joinTail;      // window this entry EXITS through
    const double  denB = (double)juce::jmax((int64_t)1, Wb - 1);
    const double  denA = (double)juce::jmax((int64_t)1, Wa - 1);
    // Window lines evaluated over the body (1.0 outside their windows).
    auto gInAt  = [&](int64_t pos) -> double {                // B line through body start
        if (entryIndex == 0 || prevT <= 0) return 1.0;
        const int64_t p = joinLeadIn + (pos - bodyStart);
        return (p >= Wb - 1) ? 1.0 : (double)p / denB;
    };
    auto gOutAt = [&](int64_t pos) -> double {                // A line through body end
        if (entry.nextLeadInLen <= 0) return 1.0;
        const int64_t winStart = bodyEnd - entry.nextLeadInLen;
        if (pos < winStart) return 1.0;
        return (double)(Wa - 1 - (pos - winStart)) / denA;
    };

    // ── Lead-in ────────────────────────────────────────────────────────────────
    // Entry 0: FULL GAIN from timeline 0 (song-start invariant, byte-locked by
    // E0LOCK). Others: gB over p = 0..L−1 — in from silence to (L−1)/(W−1) at
    // the last lead-in sample, continuing seamlessly into the body ramp below.
    if (leadInLen > 0)
    {
        const float liG0 = (entryIndex == 0) ? 1.0f : 0.0f;
        const float liG1 = (entryIndex == 0) ? 1.0f
                          : (float)((double)juce::jmax((int64_t)0, joinLeadIn - 1) / denB);

        // END-ANCHORED (2026-07-19, re-applies 2e93c22): the stretched lead-in
        // ends AT the body join — [bodyStart - sl, bodyStart). Start-anchoring
        // at (bodyStart - unstretched leadInLen) overshot into the body
        // (doubling, ratio > 1) or ended early (gap + click, ratio < 1): XTDIAG.
        // Ratio == 1 degenerates to the identical region.
        if (entry.stretchedLeadIn)
        {
            int64_t sl = (int64_t)entry.stretchedLeadIn->getNumSamples();
            mixBuf(*entry.stretchedLeadIn, bodyStart - sl, bodyStart, 0.0, liG0, liG1);
        }
        else if (std::abs(entry.leadInStretchRatio - 1.0f) < 0.0001f)
        {
            mixBuf(src, leadInStart, bodyStart, 0.0, liG0, liG1);
        }
        else
        {
            int64_t leadInTL = (int64_t)(leadInLen * entry.leadInStretchRatio + 0.5f);
            if (leadInTL > 0)
                mixBuf(src, bodyStart - leadInTL, bodyStart, 0.0, liG0, liG1);
        }
    }

    // ── Body ──────────────────────────────────────────────────────────────────
    // Continues the B window line up to gain 1.0 over the first prevTailLen
    // samples, holds 1.0, then enters the A window line over the last
    // nextLeadInLen samples. A body inside BOTH its join windows (fadeIn +
    // fadeOut > bodyLen) gets both lines — endpoint gains multiply (linear
    // approximation of the product inside the overlap region; no special
    // machinery — no harness probe triggers this geometry).
    if (bodyLen > 0)
    {
        const int64_t fadeIn  = juce::jmin(prevT, bodyLen);
        const int64_t fadeOut = juce::jmin(entry.nextLeadInLen, bodyLen);
        const int64_t m1 = bodyStart + fadeIn;
        const int64_t m2 = bodyEnd - fadeOut;
        const int64_t cuts[4] = { bodyStart, juce::jmin(m1, m2),
                                  juce::jmax(m1, m2), bodyEnd };
        for (int r = 0; r < 3; ++r) {
            const int64_t rs = cuts[r], re = cuts[r + 1];
            if (rs >= re) continue;
            const double clipOff = (double)startMark + (double)(rs - bodyStart);
            mixBuf(src, rs, re, clipOff,
                   (float)(gInAt(rs) * gOutAt(rs)),
                   (float)(gInAt(re - 1) * gOutAt(re - 1)));
        }
    }

    // ── Tail ──────────────────────────────────────────────────────────────────
    // gA over p = L..W−1: from (W−1−L)/(W−1) — continuing the body-end fade
    // without a seam — down to 0. With no successor (nextLeadInLen == 0) this
    // degenerates to the plain 1 → 0 fade (byte-locked by E0LOCK).
    if (tailLen > 0)
    {
        const float tlG0 = (float)((double)juce::jmax((int64_t)0,
                                       Wa - 1 - entry.nextLeadInLen) / denA);

        // retainTailTempo=true → always play at original speed, never use a
        // pre-stretched buffer (even if one happened to be computed).
        if (entry.retainTailTempo || !entry.stretchedTail)
        {
            mixBuf(src, bodyEnd, bodyEnd + tailLen, (double)endMark, tlG0, 0.0f);
        }
        else
        {
            int64_t sl = (int64_t)entry.stretchedTail->getNumSamples();
            mixBuf(*entry.stretchedTail, bodyEnd, bodyEnd + sl, 0.0, tlG0, 0.0f);
        }
    }
}

} // namespace BlockShuffler
