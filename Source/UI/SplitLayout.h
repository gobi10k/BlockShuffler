#pragma once
#include <juce_core/juce_core.h>

namespace BlockShuffler {

/**
 * SPLITTER (2026-08-21, Carter request 2): geometry for the draggable horizontal
 * divider between the waveform/clip area (top) and the block strip (bottom).
 *
 * Deliberately a header of PURE FUNCTIONS over juce_core only: MainComponent is
 * not buildable headlessly, but this is, so the min-size clamping that keeps the
 * block strip from collapsing is covered directly by the ResolverDiag harness
 * (T60) instead of resting on manual inspection. "Blocks invisible" has regressed
 * three times; the invariant below is the thing that must never break:
 *
 *     for ANY desired and ANY totalH > 0:
 *         blocksH = clampBlocksHeight(desired, totalH)   >= 1
 *         waveHeightFor(blocksH, totalH)                 >= 1
 *
 * and whenever the pane actually fits (totalH >= minTotalHeight()), both panes
 * additionally get at least their full minimum.
 *
 * juce::StretchableLayoutManager enforces the same two minimums during a drag,
 * but it OVERFLOWS rather than clamps when the available space is smaller than
 * the sum of the minimums — that overflow is exactly the "strip pushed off the
 * bottom edge" failure. These functions are therefore the authority MainComponent
 * lays out from; the layout manager is only the drag transport.
 */
namespace SplitLayout {

/** Height of the resizer bar itself. Comes out of the shared space. */
inline constexpr int barH = 6;

/** Waveform pane minimum. ClipWaveformView spends 54px on fixed chrome (30px
 *  add-clip button + 24px zoom bar, btnH/zoomBarH) and lays its clip rows out at
 *  rowH + rowGap = 112px. 54 + 112 = 166 is therefore the smallest pane in which
 *  a whole clip row is visible WITHOUT the waveform viewport having to scroll. */
inline constexpr int waveMinH = 166;

/** Block strip minimum. A full-size 120px tile (BlockStrip::blockH) plus the
 *  strip's padding; taller stacks shrink to BlockStrip's own 16px floor and
 *  scroll vertically, so tiles remain visible and hittable at this height. */
inline constexpr int blocksMinH = 140;

/** Default split — the pre-splitter fixed BlockStrip height, so a fresh launch
 *  and any project opened without a stored divider look exactly as before. */
inline constexpr int blocksDefaultH = 360;

/** Rejects absurd/corrupt persisted values before they reach the clamp. Larger
 *  than any pane the 2560x1600 window maximum can produce. */
inline constexpr int blocksSanityMaxH = 4096;

/** Smallest content height at which BOTH panes get their full minimum. */
inline constexpr int minTotalHeight() noexcept { return waveMinH + barH + blocksMinH; }

/**
 * Clamps a desired block-strip height into the legal range for a content area of
 * totalH pixels. Total return contract: 1 <= result <= totalH - barH - 1, so
 * neither pane can ever be laid out at zero height.
 *
 * @param desiredBlocksH  requested strip height (from a drag, or a restore)
 * @param totalH          height shared by waveform + bar + strip
 */
inline int clampBlocksHeight(int desiredBlocksH, int totalH) noexcept
{
    const int usable = totalH - barH;          // what the two panes actually share
    if (usable <= 1) return usable > 0 ? usable : 0;   // no room for two panes at all

    if (usable < waveMinH + blocksMinH)
    {
        // Degenerate: the content area is smaller than the two minimums combined
        // (waveMinH + blocksMinH = 306). Unreachable through the UI — the 800x600
        // window floor leaves totalH = 544 and usable = 538 — but reached if that
        // floor is ever bypassed, e.g. a plugin host imposing its own editor size.
        // Split proportionally
        // to the minimums and floor BOTH panes at 1px rather than overflowing the
        // strip off the bottom edge, which is what the layout manager would do.
        const int proportional = (usable * blocksMinH) / (waveMinH + blocksMinH);
        return juce::jlimit(1, usable - 1, proportional);
    }

    return juce::jlimit(blocksMinH, usable - waveMinH, desiredBlocksH);
}

/** Waveform height implied by a (already clamped) strip height. */
inline int waveHeightFor(int blocksH, int totalH) noexcept
{
    return totalH - barH - blocksH;
}

/**
 * sanitizeStoredBlocksHeight: window-independent half of the restore, used at
 * construction time when the window height is not known yet — resized() applies
 * clampBlocksHeight against the real height a moment later.
 * restoreBlocksHeight: both halves, for callers that already know totalH.
 *
 * Absent (<= 0), corrupt, or out-of-sanity-range values fall back to the default
 * split rather than being clamped to an edge — a garbage value must not silently
 * become "strip pinned to its minimum". A value that is merely too large for the
 * window it is being restored into (saved on a big screen, reopened on a small
 * one) IS clamped, since that is a real preference worth honouring as far as it
 * fits. Either way the result satisfies clampBlocksHeight's contract.
 */
inline int sanitizeStoredBlocksHeight(int storedBlocksH) noexcept
{
    return (storedBlocksH < blocksMinH || storedBlocksH > blocksSanityMaxH)
             ? blocksDefaultH : storedBlocksH;
}

inline int restoreBlocksHeight(int storedBlocksH, int totalH) noexcept
{
    return clampBlocksHeight(sanitizeStoredBlocksHeight(storedBlocksH), totalH);
}

} // namespace SplitLayout
} // namespace BlockShuffler
