#pragma once
// ── LinkArcLayout (2026-08-22, Carter link-mode render round) ────────────────
// PURE geometry for BlockLinkOverlay: where each link's arc is anchored and
// where its two label rows sit. No Graphics, no Component, no model — so the
// placement rules are testable headlessly (diag T63).
//
// GEOMETRY RULE
//   CROSS-COLUMN link (the two blocks sit in different slots, x1 != x2):
//     unchanged from the original overlay — the arc is a cubic between the two
//     tile centres at the strip's mid-height, rising to meet its label.
//
//   SAME-COLUMN link (both endpoints are members of one stack, so their centre
//   X values coincide): the old code drew cubicTo(x1,..,x2,..) with x1 == x2,
//   which degenerates into a straight vertical line THROUGH the stacked tiles.
//   Instead the arc is anchored on one SIDE of the column — at x ± colHalfW,
//   at each endpoint's own centre Y — and bows further out to an apex at
//   x ± (colHalfW + bowBase + k*bowStep). k is the index of the link among the
//   same-column links of that column, and the side alternates with k, so two
//   links in one stack never trace the same bracket. A leader line joins the
//   apex to the label.
//
//   LABELS — SPAN-ORDERED LANES (rewritten 2026-08-26 after the crowded-strip
//   defect: 5 columns with links 2<->3, 2<->5, 3<->5, 4<->5 overlapped).
//   The previous pass pushed each label up until it cleared BOTH the other
//   labels AND the reserved rects, using one predicate for both. With
//   single-block columns each tile fills the whole strip height, so the
//   reserved constraint is UNSATISFIABLE EVERYWHERE: every candidate failed,
//   every fallback lane was exhausted, and the last failed trial was kept —
//   which threw away the label-vs-label separation along with it.
//
//   So the two constraints are now ranked, not merged:
//     * label-vs-label separation is a HARD guarantee, delivered by lanes;
//     * avoiding the reserved rects is a PREFERENCE, honoured when some lane
//       allows it and dropped when none does.
//
//   Lanes are horizontal bands one label-group tall, stacked upward from just
//   above the arc band. Links are placed in order of ARC SPAN, narrowest
//   first, each taking the LOWEST lane with a free horizontal slot. Widest
//   arcs therefore end up highest and their labels nest exactly the way the
//   arcs already nest. A lane is reused whenever the x-intervals do not
//   overlap, so lane count grows with crowding, not with link count.
//
//   DEGRADATION (predictable, never silent overlap): if every lane is blocked
//   at the preferred x, the label is placed in the TOP lane and slid sideways
//   to the nearest free slot, right first then left. Only if the strip has no
//   free slot at all in that lane is the label clamped and `degraded` set, so
//   a caller can see capacity was exceeded. The bounds clamp is applied last
//   and always wins — a label outside the component is invisible.

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <cmath>

namespace BlockShuffler {
namespace LinkArcLayout {

/** One endpoint: a block tile's centre, in overlay-local coordinates. */
struct Anchor {
    float x = 0.0f;
    float y = 0.0f;
    bool  valid = false;
};

struct LinkIn {
    Anchor a, b;
    float  labelW = 0.0f;   ///< measured width of the "A <-> B" row (incl. padding)
    float  pillW  = 0.0f;   ///< measured width of the "50%" pill   (incl. padding)
};

struct Config {
    float width    = 0.0f;
    float height   = 0.0f;
    float cy       = 0.0f;   ///< strip mid-height: where cross-column arcs anchor
    float colHalfW = 50.0f;  ///< half a block tile's width

    float nameRowH = 14.0f;
    float pillH    = 15.0f;
    float rowGap   =  2.0f;

    float arcHBase = 36.0f;
    float arcHStep = 22.0f;   // retained for callers; lanes no longer stagger by index

    float laneGap  =  4.0f;   // vertical gap between lanes
    float slotGap  =  6.0f;   // horizontal gap between labels sharing a lane

    float bowBase  = 20.0f;  ///< how far past the tile edge the first bracket bows
    float bowStep  = 16.0f;  ///< extra bow per additional link in the same column

    float margin   =  2.0f;
    float pushStep = 18.0f;
    int   maxPush  = 24;
};

struct Placed {
    bool  visible    = false;
    bool  sameColumn = false;

    float anchorX1 = 0.0f, anchorY1 = 0.0f;   ///< where the arc meets the first tile
    float anchorX2 = 0.0f, anchorY2 = 0.0f;   ///< …and the second

    float apexX = 0.0f, apexY = 0.0f;         ///< same-column only: bow apex
    float arcControlY = 0.0f;                 ///< cross-column only: cubic control Y

    juce::Rectangle<float> labelBox;   ///< name row + gap + pill + plate: the collision unit
    juce::Rectangle<float> nameRect;
    juce::Rectangle<float> pillRect;

    int  lane     = 0;        ///< 0 = lowest (narrowest arc); higher = wider arc
    bool degraded = false;    ///< true only if the strip ran out of lane capacity
};

inline float totalLabelH(const Config& c) { return c.nameRowH + c.rowGap + c.pillH; }

/** Two endpoints count as the same column when their centres are within a pixel
 *  or two — stacked tiles share an X exactly, but keep a tolerance for rounding. */
inline bool isSameColumn(float x1, float x2) { return std::abs(x2 - x1) < 4.0f; }

/**
 *  @param links     one entry per project link, in project order.
 *  @param cfg       strip geometry.
 *  @param reserved  rectangles labels must avoid — the block tiles, so a label
 *                   can never land on a block's name.
 *  @return          one Placed per input, index-aligned; `visible` is false for
 *                   links whose endpoints are not both on screen.
 */
inline std::vector<Placed> layout(const std::vector<LinkIn>& links,
                                  const Config& cfg,
                                  const std::vector<juce::Rectangle<float>>& reserved)
{
    const float H = totalLabelH(cfg);
    std::vector<Placed> out(links.size());

    // Per-column counter, so several links inside one stack bow to different
    // depths and alternating sides instead of tracing the same bracket.
    std::vector<std::pair<float, int>> columnUse;   // rounded x -> count so far
    auto nextColumnIndex = [&](float x) {
        for (auto& cu : columnUse)
            if (std::abs(cu.first - x) < 4.0f) return cu.second++;
        columnUse.push_back({ x, 1 });
        return 0;
    };

    // ── 1. Arc geometry + each label's PREFERRED centre X ────────────────────
    // Unchanged from the previous revision: cross-column arcs anchor at the two
    // tile centres at mid-height, same-column arcs bracket out to the side of
    // the column. Only where the LABEL ends up is decided differently below.
    struct Item { size_t idx; float span, anchorX, boxW; };
    std::vector<Item> items;

    for (size_t i = 0; i < links.size(); ++i) {
        const auto& in = links[i];
        auto& p = out[i];
        if (!in.a.valid || !in.b.valid) continue;
        p.visible = true;

        const float boxW = juce::jmax(in.labelW, in.pillW)
                         + 4.0f;   // the drawn name plate is nameRect.expanded(2,1),
                                   // so the collision unit must cover that overhang
        p.sameColumn = isSameColumn(in.a.x, in.b.x);

        float labelAnchorX;
        if (p.sameColumn) {
            const float colX = (in.a.x + in.b.x) * 0.5f;
            const int   k    = nextColumnIndex(colX);
            const float bow  = cfg.colHalfW + cfg.bowBase + (float)k * cfg.bowStep;

            float side = ((k % 2) == 0) ? 1.0f : -1.0f;
            if (colX + bow + boxW * 0.5f > cfg.width  - cfg.margin) side = -1.0f;
            if (colX - bow - boxW * 0.5f < cfg.margin)              side =  1.0f;

            p.anchorX1 = colX + side * cfg.colHalfW;  p.anchorY1 = in.a.y;
            p.anchorX2 = colX + side * cfg.colHalfW;  p.anchorY2 = in.b.y;
            p.apexX    = colX + side * bow;
            p.apexY    = (in.a.y + in.b.y) * 0.5f;
            labelAnchorX = p.apexX;
        } else {
            p.anchorX1 = in.a.x; p.anchorY1 = cfg.cy;
            p.anchorX2 = in.b.x; p.anchorY2 = cfg.cy;
            labelAnchorX = (in.a.x + in.b.x) * 0.5f;
        }

        // A same-column bracket spans no horizontal distance, so it sorts as the
        // narrowest and settles in the lowest lane — right beside its own column.
        const float span = p.sameColumn ? 0.0f : std::abs(in.b.x - in.a.x);
        items.push_back({ i, span, labelAnchorX, boxW });
    }

    // ── 2. Lane assignment, narrowest arc first ──────────────────────────────
    // Ordering is total and deterministic (span, then anchor, then index) so the
    // same project always lays out identically.
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.span != b.span)       return a.span < b.span;
        if (a.anchorX != b.anchorX) return a.anchorX < b.anchorX;
        return a.idx < b.idx;
    });

    const float lanePitch  = H + cfg.laneGap;
    const float bandBottom = cfg.cy - cfg.arcHBase;
    auto laneTop = [&](int k) { return bandBottom - H - (float)k * lanePitch; };
    int maxLane = 0;
    while (laneTop(maxLane + 1) >= cfg.margin) ++maxLane;

    std::vector<std::vector<juce::Rectangle<float>>> laneBoxes((size_t)maxLane + 1);

    auto clampX = [&](float x, float w) {
        return juce::jlimit(cfg.margin, juce::jmax(cfg.margin, cfg.width - cfg.margin - w), x);
    };
    auto freeInLane = [&](int k, const juce::Rectangle<float>& r) {
        for (auto& placed : laneBoxes[(size_t)k])
            if (r.getRight() + cfg.slotGap > placed.getX()
             && placed.getRight() + cfg.slotGap > r.getX()) return false;
        return true;
    };
    auto hitsReserved = [&](const juce::Rectangle<float>& r) {
        for (auto& res : reserved) if (r.intersects(res)) return true;
        return false;
    };

    // Search order, most desirable first. Keeping a label centred on its own arc
    // matters more than clearing a tile (a leader line and a backing plate keep it
    // readable over one), and clearing a tile matters more than nothing at all.
    //   A: some lane, centred on the arc, clear of the tiles
    //   B: some lane, slid sideways,     clear of the tiles
    //   C: some lane, centred on the arc, tiles ignored
    //   D: some lane, slid sideways,     tiles ignored
    // Within each pass the LOWEST lane wins, so narrow arcs stay low and wide ones
    // rise — the nesting the arcs already have.
    for (const auto& it : items) {
        auto& p = out[it.idx];
        const float x0 = clampX(it.anchorX - it.boxW * 0.5f, it.boxW);

        auto tryAt = [&](int k, float x, bool needClearOfTiles,
                         juce::Rectangle<float>& hit) {
            juce::Rectangle<float> r(clampX(x, it.boxW), laneTop(k), it.boxW, H);
            if (!freeInLane(k, r)) return false;
            if (needClearOfTiles && hitsReserved(r)) return false;
            hit = r;
            return true;
        };

        juce::Rectangle<float> box;
        int  chosen = -1;
        bool found  = false;

        for (int pass = 0; pass < 4 && !found; ++pass) {
            const bool needClear = (pass < 2);
            const bool slide     = (pass % 2) == 1;
            for (int k = 0; k <= maxLane && !found; ++k) {
                if (!slide) {
                    if (tryAt(k, x0, needClear, box)) { chosen = k; found = true; }
                } else {
                    for (float step = it.boxW + cfg.slotGap;
                         step < cfg.width && !found; step += it.boxW + cfg.slotGap)
                        for (float dir : { 1.0f, -1.0f })
                            if (tryAt(k, x0 + dir * step, needClear, box)) {
                                chosen = k; found = true; break;
                            }
                }
            }
        }

        if (!found) {
            // Capacity exceeded: top lane at the preferred x, clamped. Recorded so a
            // caller can tell the strip ran out of room rather than silently guessing.
            chosen = maxLane;
            box = juce::Rectangle<float>(x0, laneTop(chosen), it.boxW, H);
            p.degraded = true;
        }

        laneBoxes[(size_t)chosen].push_back(box);
        p.lane     = chosen;
        p.labelBox = box;
        p.nameRect = juce::Rectangle<float>(box.getCentreX() - links[it.idx].labelW * 0.5f,
                                            box.getY(), links[it.idx].labelW, cfg.nameRowH);
        p.pillRect = juce::Rectangle<float>(box.getCentreX() - links[it.idx].pillW * 0.5f,
                                            box.getY() + cfg.nameRowH + cfg.rowGap,
                                            links[it.idx].pillW, cfg.pillH);
        p.arcControlY = box.getBottom() + 4.0f;
    }

    return out;
}

} // namespace LinkArcLayout
} // namespace BlockShuffler
