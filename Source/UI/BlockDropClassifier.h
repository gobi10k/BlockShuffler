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
// Kept exactly as before: dragging a stack member over a SIBLING tile rearranges
// within the stack (whole-tile hit, no zones); Shift+drag of a stacked block is a
// whole-stack move that only ever inserts, skipping its own stack's slots.

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

    if (shiftDrag && dragSg >= 0) {                     // whole-stack move: insert only
        r.action = Action::Insert; r.stackMove = true; r.index = insertIndex(dragSg);
        return r;
    }

    // Stack member over a sibling tile: rearrange within the stack (unchanged).
    if (dragSg >= 0)
        for (int i = 0; i < (int)tiles.size(); ++i)
            if (i != dragged && tiles[(size_t)i].stackGroup == dragSg && tiles[(size_t)i].rect.contains(pt)) {
                r.action = Action::RearrangeInStack; r.index = i;
                return r;
            }

    // Which other column is the point over? Columns are identified by their x.
    // The dragged block's own column (its own tile, or its own stack) is never a
    // stack target -- over it the drop inserts, as before.
    int best = -1; int bestDy = 0;
    for (int i = 0; i < (int)tiles.size(); ++i) {
        if (i == dragged) continue;
        const auto& t = tiles[(size_t)i];
        if (dragSg >= 0 && t.stackGroup == dragSg) continue;
        if (pt.x < t.rect.getX() || pt.x >= t.rect.getRight()) continue;
        const float rel = (float)(pt.x - t.rect.getX()) / (float)juce::jmax(1, t.rect.getWidth());
        if (rel < insertEdgeFrac || rel > 1.0f - insertEdgeFrac) break;   // edge of this column: INSERT
        // Middle of the column: the member under the point, else the nearest one
        // vertically (the gaps between stacked tiles still stack).
        const int dy = pt.y < t.rect.getY() ? t.rect.getY() - pt.y
                     : pt.y >= t.rect.getBottom() ? pt.y - t.rect.getBottom() + 1 : 0;
        if (best < 0 || dy < bestDy) { best = i; bestDy = dy; }
    }
    if (best >= 0) { r.action = Action::Stack; r.index = best; return r; }

    r.action = Action::Insert; r.index = insertIndex(-1);
    return r;
}

} // namespace BlockDropClassifier
} // namespace BlockShuffler
