#pragma once
#include "ArrangementResolver.h"
#include "TempoStretcher.h"

namespace BlockShuffler {

/**
 * Unified entry mixer shared by PlaybackEngine and ExportRenderer.
 *
 * Mixes one ResolvedEntry (lead-in + body + tail with gain ramps) into a
 * destination AudioBuffer.
 *
 * All timeline positions use project-sample space.  For real-time playback the
 * caller supplies pToH / hToP to convert between project and hardware sample
 * rates; pass both as 1.0 for offline rendering where the two rates are equal.
 *
 * @param entry              Entry to mix.
 * @param dest               Destination buffer.
 * @param destOriginSample   The hardware (or project-space for export) sample
 *                           index that corresponds to dest[0].
 *                           Playback: currentHead in hardware samples.
 *                           Export:   0.
 * @param destCount          Number of output samples to fill.
 * @param entryIndex         Position of this entry in the resolved arrangement.
 *                           The lead-in is skipped when entryIndex == 0 because
 *                           there is no preceding clip to crossfade with.
 * @param pToH               project-to-hardware rate ratio  (default 1.0).
 * @param hToP               hardware-to-project rate ratio  (default 1.0).
 */
inline void mixEntryToBuffer(const ResolvedEntry& entry,
                              juce::AudioBuffer<float>& dest,
                              int64_t destOriginSample,
                              int destCount,
                              int entryIndex,
                              double pToH = 1.0,
                              double hToP = 1.0)
{
    if (!entry.audioBuffer) return;
    const auto& src = *entry.audioBuffer;
    const int srcLen = src.getNumSamples();
    const int dstCh  = dest.getNumChannels();
    if (src.getNumChannels() == 0 || srcLen == 0 || dstCh == 0 || destCount <= 0) return;

    const int64_t startMark = entry.startMark;
    const int64_t endMark   = entry.endMark;
    const int64_t bodyLen   = endMark - startMark;
    const int64_t leadInLen = startMark;                                        // samples before startMark
    const int64_t tailLen   = juce::jmax((int64_t)0, (int64_t)srcLen - endMark);
    const int64_t bodyStart = entry.timelinePos;
    const int64_t bodyEnd   = bodyStart + bodyLen;
    const int64_t blockEndH = destOriginSample + (int64_t)destCount;

    // ── Inner helper ─────────────────────────────────────────────────────────
    // Mixes a sub-region of source buffer `s` into dest with a linear gain ramp.
    // regionStart/regionEnd are in project-sample space.
    // clipOff is the source-buffer index that aligns with regionStart.
    auto mixRegion = [&](const juce::AudioBuffer<float>& s,
                         int64_t regionStart, int64_t regionEnd,
                         double  clipOff,
                         float   gainStart, float gainEnd)
    {
        const int sCh  = s.getNumChannels();
        const int sLen = s.getNumSamples();
        if (sCh == 0 || sLen == 0) return;

        // Convert project region → hardware region
        const int64_t regionStartH = (int64_t)((double)regionStart * hToP + 0.5);
        const int64_t regionEndH   = (int64_t)((double)regionEnd   * hToP + 0.5);

        const int64_t ovStartH = juce::jmax(regionStartH, destOriginSample);
        const int64_t ovEndH   = juce::jmin(regionEndH,   blockEndH);
        if (ovStartH >= ovEndH) return;

        const int    destOff  = (int)(ovStartH - destOriginSample);
        const int    destCt   = (int)(ovEndH   - ovStartH);

        // Corresponding project-space window → source position + sample count
        const double pOvStart  = (double)ovStartH * pToH;
        double       srcStart  = clipOff + (pOvStart - (double)regionStart);
        double       srcSamps  = (double)destCt * pToH;

        if (srcStart + srcSamps <= 0.0 || srcStart >= (double)sLen) return;
        if (srcStart < 0.0) {
            srcSamps += srcStart;   // trim leading out-of-bounds
            srcStart  = 0.0;
        }

        // Interpolate the gain ramp to the exact window we're writing
        const int64_t regLen = regionEnd - regionStart;
        float gs = gainStart, ge = gainEnd;
        if (regLen > 1) {
            const float t0 = (float)(pOvStart - (double)regionStart) / (float)regLen;
            const float t1 = (float)((double)pOvStart + srcSamps - (double)regionStart) / (float)regLen;
            gs = gainStart + t0 * (gainEnd - gainStart);
            ge = gainStart + t1 * (gainEnd - gainStart);
        }
        gs *= entry.gain;
        ge *= entry.gain;

        TempoStretcher::resampleAdd(s, srcStart, srcSamps, dest, destOff, destCt, gs, ge);
    };

    // ── Lead-in ───────────────────────────────────────────────────────────────
    // Skip for entry 0: there is no preceding clip to crossfade with.
    // Entries 1+ fade in from 0 → 1 over [timelinePos - leadInTL, timelinePos).
    if (entryIndex > 0 && leadInLen > 0)
    {
        if (entry.stretchedLeadIn)
        {
            // Pre-computed WSOLA buffer: timeline [bodyStart - sl, bodyStart)
            const int64_t sl = (int64_t)entry.stretchedLeadIn->getNumSamples();
            mixRegion(*entry.stretchedLeadIn, bodyStart - sl, bodyStart, 0.0, 0.0f, 1.0f);
        }
        else if (std::abs(entry.leadInStretchRatio - 1.0f) < 0.0001f)
        {
            // Ratio is 1.0 — no stretching, use original samples directly
            mixRegion(src, bodyStart - leadInLen, bodyStart, 0.0, 0.0f, 1.0f);
        }
        else
        {
            // Fallback: WSOLA was bypassed or failed; use linear-interp resampling
            const int64_t leadInTL = (int64_t)std::llround((double)leadInLen * entry.leadInStretchRatio);
            mixRegion(src, bodyStart - leadInTL, bodyStart, 0.0, 0.0f, 1.0f);
        }
    }

    // ── Body ──────────────────────────────────────────────────────────────────
    if (bodyLen > 0)
        mixRegion(src, bodyStart, bodyEnd, (double)startMark, 1.0f, 1.0f);

    // ── Tail ──────────────────────────────────────────────────────────────────
    // Fades out from 1 → 0.  retainTailTempo skips the pre-stretched buffer and
    // uses the original audio at its native speed regardless.
    if (tailLen > 0)
    {
        if (entry.retainTailTempo || !entry.stretchedTail)
        {
            mixRegion(src, bodyEnd, bodyEnd + tailLen, (double)endMark, 1.0f, 0.0f);
        }
        else
        {
            const int64_t sl = (int64_t)entry.stretchedTail->getNumSamples();
            mixRegion(*entry.stretchedTail, bodyEnd, bodyEnd + sl, 0.0, 1.0f, 0.0f);
        }
    }
}

} // namespace BlockShuffler
