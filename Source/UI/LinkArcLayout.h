#pragma once
// ── LinkArcLayout (2026-08-22, Carter link-mode render round) ────────────────
// PURE geometry for BlockLinkOverlay: where each link's arc is anchored and
// where its two label rows sit. No Graphics, no Component, no model — so the
// placement rules are testable headlessly (diag T63/T65/T66).
//
// GEOMETRY RULE (unchanged since round 3)
//   CROSS-COLUMN link (the two blocks sit in different slots, x1 != x2):
//     the arc is a cubic between the two tile centres at the strip's mid-height,
//     rising to meet its label.
//
//   SAME-COLUMN link (both endpoints are members of one stack, so their centre
//   X values coincide): a straight cubic would degenerate into a vertical line
//   THROUGH the stacked tiles. Instead the arc is anchored on one SIDE of the
//   column — at x ± colHalfW, at each endpoint's own centre Y — and bows further
//   out to an apex at x ± (colHalfW + bowBase + k*bowStep). k is the index of the
//   link among the same-column links of that column, and the side alternates with
//   k, so two links in one stack never trace the same bracket.
//
// LABEL RULE (rewritten 2026-08-30 — Carter rejected the lane layout of 74a439d)
//
//   THE RULE, in one line: every label starts at its OWN arc's natural position,
//   and only a label that would actually overlap another label is moved, by the
//   smallest displacement that clears it.
//
//   Natural position:
//     * CROSS-COLUMN — centred on the arc's midpoint X, riding just above the
//       arc band at cy - (arcHBase + i*arcHStep) - H - 4, i.e. each link's arc
//       and its label rise together. This is exactly the 86cc4b6 placement.
//     * SAME-COLUMN — BESIDE the bow apex and OUTSIDE the stack column, on the
//       side the bow points, vertically CENTRED on the apex (Carter's sketch,
//       2026-08-30). It no longer floats above the tiles as it did at 86cc4b6.
//
//   Displacement, only for labels that collide, nearest-first: candidates are
//   ranked by how far they move the label (vertical push weighted 1.0,
//   horizontal slide 0.5, so a label prefers to rise over its own arc before it
//   slides off it) and the CHEAPEST candidate that is free wins. Lanes are what
//   this degenerates into when a strip is crowded — a consequence, not the layout.
//
//   TWO CONSTRAINTS, RANKED (the ranking is what 74a439d got right and is kept):
//     * label-vs-label separation is a HARD guarantee;
//     * clearing the block tiles is a PREFERENCE.
//   So the candidate list is walked twice: once demanding the tiles be clear,
//   then, only if that found nothing, once ignoring them. With single-block
//   columns each tile fills the whole strip height and the tile constraint is
//   unsatisfiable everywhere — that must never be allowed to destroy separation.
//
//   NO FAILED TRIAL IS EVER KEPT. This is the -104 bug of 86cc4b6: `box = trial`
//   sat at the bottom of the search loop, so when every candidate failed the LAST
//   REJECTED one was written out, shifting every label by the same amount and
//   stacking them on top of each other. Here a box is only ever assigned from a
//   candidate that PASSED. If no candidate passes in either walk, the label is
//   not drawn at all (labelVisible = false, degraded = true) — the arc still is.
//   An undrawn label is a visible loss of information; two labels drawn on top of
//   each other are a visible loss of information AND a lie about which is which.
//
//   The bounds clamp is applied to every candidate BEFORE it is tested, so a
//   label that passes is both inside the strip and clear of its neighbours.

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
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

    float arcHBase = 36.0f;  ///< height of the first cross-column arc above cy
    float arcHStep = 22.0f;  ///< extra height per subsequent link, so arcs nest

    float bowBase  = 20.0f;  ///< how far past the tile edge the first bracket bows
    float bowStep  = 16.0f;  ///< extra bow per additional link in the same column
    float labelGap =  6.0f;  ///< gap between a same-column bow apex and its label

    float slotGap  =  6.0f;  ///< clear space required between two label groups
    float margin   =  2.0f;

    float pushStep = 18.0f;  ///< one vertical displacement step
    int   maxPush  = 24;     ///< most steps a label may be pushed up
    float slideStep=  8.0f;  ///< one horizontal displacement step

    /** What clearing a block tile is WORTH, in pixels of displacement. A label
     *  will move up to this far to get off a tile and no further: past that the
     *  backing plate is the better answer, because a label dragged across the
     *  strip is harder to read than one sitting on a tile it is plated over. */
    float tilePenalty = 70.0f;
};

struct Placed {
    bool  visible    = false;   ///< both endpoints on screen: draw the ARC
    bool  sameColumn = false;

    float anchorX1 = 0.0f, anchorY1 = 0.0f;   ///< where the arc meets the first tile
    float anchorX2 = 0.0f, anchorY2 = 0.0f;   ///< …and the second

    float apexX = 0.0f, apexY = 0.0f;         ///< same-column only: bow apex
    float arcControlY = 0.0f;                 ///< cross-column only: cubic control Y

    juce::Rectangle<float> labelBox;   ///< name row + gap + pill + plate: the collision unit
    juce::Rectangle<float> nameRect;
    juce::Rectangle<float> pillRect;

    bool labelVisible = false;  ///< false only when no free position existed at all
    bool displaced    = false;  ///< true when a collision moved it off its natural spot
    bool degraded     = false;  ///< true only if the strip ran out of room entirely
};

inline float totalLabelH(const Config& c) { return c.nameRowH + c.rowGap + c.pillH; }

/** Two endpoints count as the same column when their centres are within a pixel
 *  or two — stacked tiles share an X exactly, but keep a tolerance for rounding. */
inline bool isSameColumn(float x1, float x2) { return std::abs(x2 - x1) < 4.0f; }

/**
 *  @param links     one entry per project link, in project order.
 *  @param cfg       strip geometry.
 *  @param reserved  rectangles labels should avoid — the block tiles, so a label
 *                   does not land on a block's name. A PREFERENCE, not a
 *                   guarantee: see the ranking note at the top of this file.
 *  @return          one Placed per input, index-aligned; `visible` is false for
 *                   links whose endpoints are not both on screen.
 */
inline std::vector<Placed> layout(const std::vector<LinkIn>& links,
                                  const Config& cfg,
                                  const std::vector<juce::Rectangle<float>>& reserved)
{
    const float H = totalLabelH(cfg);
    std::vector<Placed> out(links.size());

    // Committed label boxes. A box only ever lands here after it has been proven
    // free of every box already in it, which is what makes separation a guarantee
    // rather than an outcome.
    std::vector<juce::Rectangle<float>> committed;

    // Per-column counter, so several links inside one stack bow to different
    // depths and alternating sides instead of tracing the same bracket.
    std::vector<std::pair<float, int>> columnUse;   // rounded x -> count so far
    auto nextColumnIndex = [&](float x) {
        for (auto& cu : columnUse)
            if (std::abs(cu.first - x) < 4.0f) return cu.second++;
        columnUse.push_back({ x, 1 });
        return 0;
    };

    auto clampBox = [&](juce::Rectangle<float> r) {
        if (r.getRight()  > cfg.width  - cfg.margin) r.setX(cfg.width  - cfg.margin - r.getWidth());
        if (r.getX()      < cfg.margin)              r.setX(cfg.margin);
        if (r.getBottom() > cfg.height - cfg.margin) r.setY(cfg.height - cfg.margin - r.getHeight());
        if (r.getY()      < cfg.margin)              r.setY(cfg.margin);
        return r;
    };
    auto freeOfLabels = [&](const juce::Rectangle<float>& r) {
        for (auto& c : committed)
            if (r.getRight()  + cfg.slotGap > c.getX()
             && c.getRight()  + cfg.slotGap > r.getX()
             && r.getBottom() > c.getY()
             && c.getBottom() > r.getY()) return false;
        return true;
    };
    auto clearOfTiles = [&](const juce::Rectangle<float>& r) {
        for (auto& t : reserved) if (r.intersects(t)) return false;
        return true;
    };

    for (size_t i = 0; i < links.size(); ++i) {
        const auto& in = links[i];
        auto& p = out[i];
        if (!in.a.valid || !in.b.valid) continue;
        p.visible = true;

        // The drawn name plate is nameRect.expanded(2,1), so the collision unit
        // must cover that overhang or two labels can touch plate-to-plate.
        const float boxW = juce::jmax(in.labelW, in.pillW) + 4.0f;
        p.sameColumn = isSameColumn(in.a.x, in.b.x);

        // ── Arc geometry + this label's NATURAL position ─────────────────────
        float naturalX, naturalTop;
        if (p.sameColumn) {
            const float colX = (in.a.x + in.b.x) * 0.5f;
            const int   k    = nextColumnIndex(colX);
            const float bow  = cfg.colHalfW + cfg.bowBase + (float)k * cfg.bowStep;

            // Side/depth choice is round-3 arc geometry and is left exactly as it
            // was: the label clamp, not the bow, absorbs a tight edge.
            float side = ((k % 2) == 0) ? 1.0f : -1.0f;
            if (colX + bow + boxW * 0.5f > cfg.width  - cfg.margin) side = -1.0f;
            if (colX - bow - boxW * 0.5f < cfg.margin)              side =  1.0f;

            p.anchorX1 = colX + side * cfg.colHalfW;  p.anchorY1 = in.a.y;
            p.anchorX2 = colX + side * cfg.colHalfW;  p.anchorY2 = in.b.y;
            p.apexX    = colX + side * bow;
            p.apexY    = (in.a.y + in.b.y) * 0.5f;

            // Carter's sketch: BESIDE the apex, at the apex's height, outside the
            // column — not stacked above the tiles.
            naturalX   = p.apexX + side * (cfg.labelGap + boxW * 0.5f) - boxW * 0.5f;
            naturalTop = p.apexY - H * 0.5f;
        } else {
            p.anchorX1 = in.a.x; p.anchorY1 = cfg.cy;
            p.anchorX2 = in.b.x; p.anchorY2 = cfg.cy;

            // 86cc4b6's placement, restored verbatim: centred on the arc midpoint,
            // riding just above an arc band that steps up with the link index.
            naturalX   = (in.a.x + in.b.x) * 0.5f - boxW * 0.5f;
            naturalTop = cfg.cy - (cfg.arcHBase + (float)i * cfg.arcHStep) - H - 4.0f;
        }

        const auto naturalBox = clampBox({ naturalX, naturalTop, boxW, H });

        // The arc is drawn to its OWN natural height whatever happens to the
        // label, so a displaced label drags a leader line rather than the arc.
        p.arcControlY = naturalBox.getBottom() + 4.0f;

        // ── Displacement search, nearest first ───────────────────────────────
        // Candidates are ranked by distance from natural. Vertical is cheaper than
        // horizontal so a crowded label rises over its own arc before it slides
        // away from it — which is how the lanes people liked emerge naturally.
        struct Cand { float cost; float dy, dx; };
        std::vector<Cand> cands;
        const int slideMax = (int)std::ceil(cfg.width  / juce::jmax(1.0f, cfg.slideStep));
        const int pushMax  = (int)std::ceil(cfg.height / juce::jmax(1.0f, cfg.pushStep));
        const int upMax    = juce::jmax(cfg.maxPush, pushMax);
        for (int sy = -upMax; sy <= pushMax; ++sy) {
            const float dy = (float)sy * cfg.pushStep;
            // Up is cheaper than down: a label rises over its own arc the way the
            // arcs already nest, and only drops below it when the strip is so short
            // that the space above is spent (a 140px strip clamps every natural
            // position to the top margin, and downward is then the ONLY room left).
            const float dyCost = std::abs(dy) * (sy <= 0 ? 1.0f : 1.25f);
            for (int sx = 0; sx <= slideMax; ++sx) {
                const float dx = (float)sx * cfg.slideStep;
                const float cost = dyCost + dx * 0.5f;
                if (sx == 0) { cands.push_back({ cost, dy, 0.0f }); continue; }
                cands.push_back({ cost, dy,  dx });
                cands.push_back({ cost, dy, -dx });
            }
        }
        std::stable_sort(cands.begin(), cands.end(),
                         [](const Cand& l, const Cand& r) { return l.cost < r.cost; });

        // The two constraints stay RANKED, but the preference is now PRICED rather
        // than absolute. An earlier revision walked all candidates demanding clear
        // tiles first, which with single-block columns (tiles fill the whole strip
        // height) dragged every label past the last column on a 200px leader --
        // technically clear of the tiles, useless to read. Landing on a tile now
        // costs cfg.tilePenalty pixels of displacement, so a label will step aside
        // for a tile but will not emigrate for one.
        //
        // Separation stays a HARD guarantee: a candidate that is not free of the
        // labels already committed is never costed and never chosen. Candidates are
        // pre-sorted by displacement cost, and the tile penalty is non-negative, so
        // once the best total found is <= the next candidate's base cost no later
        // candidate can beat it -- that is the early exit.
        juce::Rectangle<float> box;
        bool  settled  = false;
        float bestCost = 0.0f;
        for (const auto& c : cands) {
            if (settled && c.cost >= bestCost) break;
            const auto trial = clampBox({ naturalBox.getX() + c.dx,
                                          naturalBox.getY() + c.dy, boxW, H });
            if (!freeOfLabels(trial)) continue;
            const float total = c.cost + (clearOfTiles(trial) ? 0.0f : cfg.tilePenalty);
            if (settled && total >= bestCost) continue;
            box      = trial;        // assigned ONLY from a candidate that PASSED the
            bestCost = total;        // hard test -- never a rejected trial (-104 bug).
            settled  = true;
        }

        if (!settled) {
            // Nowhere in the strip can this label go without covering another one.
            // Say so; do not draw it, and do not corrupt anyone else's placement.
            p.labelVisible = false;
            p.degraded     = true;
            p.labelBox     = {};
            continue;
        }

        committed.push_back(box);
        p.labelVisible = true;
        p.displaced    = std::abs(box.getX() - naturalBox.getX()) > 0.5f
                      || std::abs(box.getY() - naturalBox.getY()) > 0.5f;
        p.labelBox     = box;
        p.nameRect     = juce::Rectangle<float>(box.getCentreX() - in.labelW * 0.5f,
                                                box.getY(), in.labelW, cfg.nameRowH);
        p.pillRect     = juce::Rectangle<float>(box.getCentreX() - in.pillW * 0.5f,
                                                box.getY() + cfg.nameRowH + cfg.rowGap,
                                                in.pillW, cfg.pillH);
    }

    return out;
}

} // namespace LinkArcLayout
} // namespace BlockShuffler
