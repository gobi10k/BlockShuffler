#pragma once
// ── BlockDropClassifier (2026-11-13) ─────────────────────────────────────────
// THE ONLY place a block drag decides STACK vs INSERT (vs rearrange-in-stack).
// BlockStrip::updateDragFeedback() calls it for the hover preview, and
// BlockStrip::blockDropped() commits whatever updateDragFeedback() left behind,
// so the preview and the drop cannot disagree.
//
// ZONES (per target column, horizontally):
//   | INSERT before | ------ STACK ------ | INSERT after |
//   |     25%       |         50%         |     25%      |
// Gaps between columns and the space past the last one are INSERT (append).
// Before this, the WHOLE 100px tile stacked and only the 10px gap inserted, so
// blocks got stacked by accident; and in a stack column only the tiles counted,
// so the 4px gaps between stacked tiles silently turned into INSERT.
//
// STACK TARGET (2026-11-23): the member the dropped block goes directly BELOW --
// the tile under the point, or, in the gap between two stacked tiles, the tile
// ABOVE the gap (above the column's first tile: that first tile). Never "nearest",
// which flipped to the lower tile halfway through a gap.
//
// Kept exactly as before: dragging a stack member over a SIBLING tile rearranges
// within the stack (whole-tile hit, no zones). Shift+drag of a stacked block is a
// whole-stack move, skipping its own stack's slots: it inserts, EXCEPT in the
// stack zone over a member of ANOTHER stack, where the whole stack goes below
// that member (Stack + stackMove). Over an unstacked block it still inserts.

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace BlockShuffler {
namespace BlockDropClassifier {

/** Fraction of a target column, on EACH side, that inserts rather than stacks. */
inline constexpr float insertEdgeFrac = 0.25f;

enum class Action { None, Insert, Stack, RearrangeInStack };

struct Tile {
    juce::Rectangle<int> rect;   ///< the tile's laid-out (pre-drag) bounds
    int stackGroup = -1;
};

struct Result {
    Action action = Action::None;
    int    index  = -1;      ///< Stack/Rearrange: target block index. Insert: insert-before index.
    bool   stackMove = false;
};

/** @param pt          the drag point (the dragged tile's centre), contentArea coords
 *  @param tiles       one per project block, in blocks[] order
 *  @param dragged     index of the dragged block in tiles
 *  @param shiftDrag   Shift held at drag start */
inline Result classify(juce::Point<int> pt, const std::vector<Tile>& tiles, int dragged, bool shiftDrag)
{
    Result r;
    if (dragged < 0 || dragged >= (int)tiles.size()) return r;
    const int dragSg = tiles[(size_t)dragged].stackGroup;

    // "Insert before the first block (other than the dragged one, and other than
    // `skipSg`'s members) whose centre is right of the point" -- the original rule.
    auto insertIndex = [&](int skipSg) {
        for (int i = 0; i < (int)tiles.size(); ++i) {
            if (i == dragged) continue;
            if (skipSg >= 0 && tiles[(size_t)i].stackGroup == skipSg) continue;
            if (tiles[(size_t)i].rect.getCentreX() > pt.x) return i;
        }
        return (int)tiles.size();
    };

    const bool stackMove = shiftDrag && dragSg >= 0;   // whole-stack move

    // Stack member over a sibling tile: rearrange within the stack (unchanged).
    if (dragSg >= 0 && !stackMove)
        for (int i = 0; i < (int)tiles.size(); ++i)
            if (i != dragged && tiles[(size_t)i].stackGroup == dragSg && tiles[(size_t)i].rect.contains(pt)) {
                r.action = Action::RearrangeInStack; r.index = i;
                return r;
            }

    // Which other column is the point over? Columns are identified by their x.
    // The dragged block's own column (its own tile, or its own stack) is never a
    // stack target -- over it the drop inserts, as before.
    int best = -1, top = -1;
    for (int i = 0; i < (int)tiles.size(); ++i) {
        if (i == dragged) continue;
        const auto& t = tiles[(size_t)i];
        if (dragSg >= 0 && t.stackGroup == dragSg) continue;
        if (pt.x < t.rect.getX() || pt.x >= t.rect.getRight()) continue;
        const float rel = (float)(pt.x - t.rect.getX()) / (float)juce::jmax(1, t.rect.getWidth());
        if (rel < insertEdgeFrac || rel > 1.0f - insertEdgeFrac) { best = top = -1; break; }   // edge: INSERT
        // Middle of the column: the lowest member whose top is at/above the point
        // (the tile under it, or the tile above a gap); else the topmost member.
        if (t.rect.getY() <= pt.y && (best < 0 || t.rect.getY() > tiles[(size_t)best].rect.getY())) best = i;
        if (top < 0 || t.rect.getY() < tiles[(size_t)top].rect.getY()) top = i;
    }
    if (best < 0) best = top;
    // A whole-stack move only merges into ANOTHER stack; over an unstacked block it inserts.
    if (best >= 0 && !(stackMove && tiles[(size_t)best].stackGroup < 0)) {
        r.action = Action::Stack; r.index = best; r.stackMove = stackMove; return r;
    }

    r.action = Action::Insert; r.stackMove = stackMove; r.index = insertIndex(stackMove ? dragSg : -1);
    return r;
}

} // namespace BlockDropClassifier
} // namespace BlockShuffler
