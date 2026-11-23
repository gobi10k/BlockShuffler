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

/** One endpoint: a block tile's centre, in overlay-local coordinates.
 *  `stacked` + `tile` (2026-11-13): a CROSS-column link whose endpoint is a
 *  stack member is anchored on THAT member tile's side edge, not at the column
 *  centre at strip mid-height (which every member of a stack shared, so all
 *  links between two stacks were drawn as one identical curve). */
struct Anchor {
    float x = 0.0f;
    float y = 0.0f;
    bool  valid = false;
    bool  stacked = false;            ///< endpoint is a member of a stack
    juce::Rectangle<float> tile;      ///< that member's tile rect (needed when stacked)
};

struct LinkIn {
    Anchor a, b;
    /** Fixed per-link tie-break for the arc direction rule (2026-11-23): used only
     *  when a stack-touching link's anchors average EXACTLY on the strip centre. */
    bool tieBulgesDown = false;
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

    /** The part of the overlay the user can actually SEE: the block strip's
     *  viewport, in overlay-local coordinates. The strip scrolls horizontally
     *  under a fixed overlay, so a link whose blocks have scrolled away has an
     *  arc that is off-screen even though its anchors are still valid numbers.
     *  Left empty it means "the whole strip", which is what every caller and
     *  test that predates scrolling wants. */
    juce::Rectangle<float> viewport;

    float pushStep = 18.0f;  ///< one vertical displacement step
    int   maxPush  = 24;     ///< most steps a label may be pushed up
    float slideStep=  8.0f;  ///< one horizontal displacement step

    /** What clearing a block tile is WORTH, in pixels of displacement. A label
     *  will move up to this far to get off a tile and no further: past that the
     *  backing plate is the better answer, because a label dragged across the
     *  strip is harder to read than one sitting on a tile it is plated over. */
    float tilePenalty = 70.0f;

    /** Space no label may cover, treated exactly like an already-placed label
     *  (2026-11-23: the link/stack-mode banner "Click a block to link to it"). */
    std::vector<juce::Rectangle<float>> keepOut;

    // ── Cross-column links (2026-11-13) ──────────────────────────────────────
    float anchorSpread = 6.0f;  ///< spacing between links leaving the same tile edge
    float labelVGap    = 2.0f;  ///< vertical clearance between label boxes (plates overhang 1px)
    float onCurveTol   = 6.0f;  ///< label centre within this of its curve = "on its curve"
    float tMin = 0.25f, tMax = 0.75f, tStep = 0.05f;  ///< slide range along the curve
    /** A stack-member end leaves its tile's SIDE edge, so its control point is
     *  pushed outward by this fraction of the span (capped) -- otherwise the
     *  reference cubic's vertical end tangent runs the curve straight up the
     *  tile border. Unstacked ends keep offset 0 (the reference arch). */
    float edgeCtrlFrac = 0.3f, edgeCtrlMax = 60.0f;
};

struct Placed {
    bool  visible    = false;   ///< both endpoints on screen: draw the ARC
    bool  sameColumn = false;

    float anchorX1 = 0.0f, anchorY1 = 0.0f;   ///< where the arc meets the first tile
    float anchorX2 = 0.0f, anchorY2 = 0.0f;   ///< …and the second

    float apexX = 0.0f, apexY = 0.0f;         ///< same-column only: bow apex
    float arcControlY = 0.0f;                 ///< cross-column only: cubic control Y
    float midX = 0.0f, midY = 0.0f;           ///< cross-column only: curve point at t=0.5 (leader target)
    float ctrlX1 = 0.0f, ctrlX2 = 0.0f;       ///< cross-column only: cubic control X (== anchor X for an unstacked end)

    juce::Rectangle<float> labelBox;   ///< name row + gap + pill + plate: the collision unit
    juce::Rectangle<float> nameRect;
    juce::Rectangle<float> pillRect;

    bool labelVisible = false;  ///< false when culled off-viewport, or when no free position existed
    bool displaced    = false;  ///< true when a collision moved it off its natural spot
    bool degraded     = false;  ///< true only if the strip ran out of room entirely
    bool offViewport  = false;  ///< true when the ARC is off-screen: culled, not dropped
};

inline float totalLabelH(const Config& c) { return c.nameRowH + c.rowGap + c.pillH; }

/** Two endpoints count as the same column when their centres are within a pixel
 *  or two — stacked tiles share an X exactly, but keep a tolerance for rounding. */
inline bool isSameColumn(float x1, float x2) { return std::abs(x2 - x1) < 4.0f; }

/** Point on a cross-column arc: the cubic BlockLinkOverlay::paint strokes,
 *  P0=(x1,y1) P1=(ctrlX1,cY) P2=(ctrlX2,cY) P3=(x2,y2). */
inline juce::Point<float> crossCurvePoint(const Placed& p, float t) {
    const float u = 1.0f - t;
    const float a = u * u * u, b = 3.0f * u * u * t, c = 3.0f * u * t * t, d = t * t * t;
    return { a * p.anchorX1 + b * p.ctrlX1 + c * p.ctrlX2 + d * p.anchorX2,
             a * p.anchorY1 + (b + c) * p.arcControlY + d * p.anchorY2 };
}

/** Distance from a point to a cross-column arc (sampled; plenty for a 6px test). */
inline float distanceToCrossCurve(const Placed& p, juce::Point<float> q) {
    float best = 1e9f;
    for (int k = 0; k <= 200; ++k)
        best = juce::jmin(best, q.getDistanceFrom(crossCurvePoint(p, (float)k / 200.0f)));
    return best;
}

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
    std::vector<juce::Rectangle<float>> committed(cfg.keepOut.begin(), cfg.keepOut.end());

    // Per-column counter, so several links inside one stack bow to different
    // depths and alternating sides instead of tracing the same bracket.
    std::vector<std::pair<float, int>> columnUse;   // rounded x -> count so far
    auto nextColumnIndex = [&](float x) {
        for (auto& cu : columnUse)
            if (std::abs(cu.first - x) < 4.0f) return cu.second++;
        columnUse.push_back({ x, 1 });
        return 0;
    };

    // What the user can actually see. Empty means "the whole strip", which is what
    // every caller that does not scroll wants.
    const juce::Rectangle<float> vis = cfg.viewport.isEmpty()
        ? juce::Rectangle<float>(0.0f, 0.0f, cfg.width, cfg.height)
        : cfg.viewport;

    // The clamp is a READABILITY NICETY and nothing more. It used to be applied
    // last and given priority over everything, which is what pinned a label to the
    // strip edge after its blocks had scrolled away (Carter, 4a5d1b3): the arc was
    // gone and the label was still there, clamped into view on its own.
    // Now it clamps into the VISIBLE area, and where the arc runs off that area it
    // may not drag the label past the part of the arc still on screen -- a label
    // whose arc is half off-screen goes half off-screen with it and is clipped.
    auto clampBox = [&](juce::Rectangle<float> r,
                        const juce::Rectangle<float>& arcVisible,
                        bool arcRunsOffViewport) {
        if (arcRunsOffViewport) {
            // The arc itself is running off the edge. The label goes with it: its
            // centre stays over the part of the arc that is still on screen and the
            // box is left to be CLIPPED. The horizontal viewport clamp is skipped
            // outright here -- pushing the label inward so it stays wholly visible
            // is precisely what stranded it at the strip edge.
            const float lo = arcVisible.getX()     - r.getWidth() * 0.5f;
            const float hi = arcVisible.getRight() - r.getWidth() * 0.5f;
            r.setX(juce::jlimit(juce::jmin(lo, hi), juce::jmax(lo, hi), r.getX()));
        } else {
            if (r.getRight() > vis.getRight() - cfg.margin) r.setX(vis.getRight() - cfg.margin - r.getWidth());
            if (r.getX()     < vis.getX()     + cfg.margin) r.setX(vis.getX()     + cfg.margin);
        }
        // Vertical always: the strip scrolls sideways, so a label is never pushed
        // off the top or bottom by scrolling and the old clamp still applies.
        if (r.getBottom() > vis.getBottom() - cfg.margin) r.setY(vis.getBottom() - cfg.margin - r.getHeight());
        if (r.getY()      < vis.getY()      + cfg.margin) r.setY(vis.getY()      + cfg.margin);
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

    // ── PASS 1: SAME-COLUMN links (inside one stack). Behaviour identical to the
    // pre-2026-11-13 loop for these links (only its now-unreachable cross-column
    // branch was removed). Placed first so a cross-column label can never
    // displace one of them.
    for (size_t i = 0; i < links.size(); ++i) {
        const auto& in = links[i];
        auto& p = out[i];
        if (!in.a.valid || !in.b.valid) continue;
        if (!isSameColumn(in.a.x, in.b.x)) continue;   // pass 2 handles these
        p.visible = true;

        // The drawn name plate is nameRect.expanded(2,1), so the collision unit
        // must cover that overhang or two labels can touch plate-to-plate.
        const float boxW = juce::jmax(in.labelW, in.pillW) + 4.0f;
        p.sameColumn = true;

        // ── Arc geometry + this label's NATURAL position ─────────────────────
        float naturalX, naturalTop;
        {
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
        }

        // ── Is this link's arc on screen at all? ─────────────────────────────
        // BlockStrip hands us anchors with the viewport's scroll already taken out,
        // so a block that has scrolled off simply has a negative centre. The arc is
        // then drawn off-screen and everything that belongs to it must go with it.
        const juce::Rectangle<float> arcBounds = juce::Rectangle<float>::leftTopRightBottom(
                  juce::jmin(p.anchorX1, p.apexX), juce::jmin(p.anchorY1, p.anchorY2),
                  juce::jmax(p.anchorX1, p.apexX), juce::jmax(p.anchorY1, p.anchorY2));

        if (!arcBounds.intersects(vis)) {
            // Scrolled away. No clamp, no plate, no pill, no leader -- and nothing
            // pushed into `committed`, so it cannot displace a label that IS on
            // screen. Culling only ever gives the visible labels more room.
            p.labelVisible = false;
            p.offViewport  = true;
            p.labelBox     = {};
            continue;
        }

        const auto arcVis = arcBounds.getIntersection(vis);
        // Horizontal only: the strip scrolls sideways, and a vertical overhang is
        // the ordinary "tall arc" case the clamp has always handled.
        const bool arcRunsOff = arcBounds.getX() < vis.getX()
                             || arcBounds.getRight() > vis.getRight();

        const auto naturalBox = clampBox({ naturalX, naturalTop, boxW, H },
                                         arcVis, arcRunsOff);

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
                                          naturalBox.getY() + c.dy, boxW, H },
                                        arcVis, arcRunsOff);
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


    // ── PASS 2: CROSS-COLUMN links (2026-11-13) ──────────────────────────────
    // ANCHORS. An unstacked endpoint keeps the reference rule: its tile centre X
    // at strip mid-height (a single-block tile fills the strip, so that point is
    // its middle). A STACK MEMBER is anchored on its own tile's side edge facing
    // the other end. Several links leaving the same edge are spread
    // cfg.anchorSpread apart, never at the edge's centre Y -- that is exactly
    // where pass 1's same-column brackets meet the edge.
    std::vector<std::pair<juce::Point<float>, int>> edgeUse;   // edge key -> uses
    auto nextEdgeUse = [&](juce::Point<float> key) {
        for (auto& e : edgeUse)
            if (e.first.getDistanceFrom(key) < 0.5f) return e.second++;
        edgeUse.push_back({ key, 1 });
        return 0;
    };
    auto isMember = [](const Anchor& a) { return a.stacked && !a.tile.isEmpty(); };
    auto endpoint = [&](const Anchor& self) -> juce::Point<float> {   // unstacked end
        const float s = cfg.anchorSpread;
        const int k = nextEdgeUse({ self.x, cfg.cy });
        const float d = (k == 0) ? 0.0f : (float)((k + 1) / 2) * s * ((k % 2) ? 1.0f : -1.0f);
        return { self.x + d, cfg.cy };                     // k=0: the reference point exactly
    };

    // STACK-MEMBER ends (2026-11-23): on the side edge of THAT member's tile facing
    // the other end, centred on the tile's vertical centre; several ends on one
    // edge spread cfg.anchorSpread apart, ordered by the other end's height so
    // they do not cross on the way out. memberEnd[2*i + (0 = a, 1 = b)].
    std::vector<juce::Point<float>> memberEnd(links.size() * 2);
    {
        struct Use { float ex, ey, otherY; size_t slot; juce::Rectangle<float> tile; };
        std::vector<Use> uses;
        for (size_t i = 0; i < links.size(); ++i) {
            const auto& in = links[i];
            if (!in.a.valid || !in.b.valid || isSameColumn(in.a.x, in.b.x)) continue;
            for (int e = 0; e < 2; ++e) {
                const auto& self  = e ? in.b : in.a;
                const auto& other = e ? in.a : in.b;
                if (!isMember(self)) continue;
                const float ex = other.x >= self.x ? self.tile.getRight() : self.tile.getX();
                uses.push_back({ ex, self.tile.getCentreY(), other.y, 2 * i + (size_t)e, self.tile });
            }
        }
        std::vector<bool> done(uses.size(), false);
        for (size_t u = 0; u < uses.size(); ++u) {
            if (done[u]) continue;
            std::vector<size_t> group;
            for (size_t v = u; v < uses.size(); ++v)
                if (!done[v] && std::abs(uses[v].ex - uses[u].ex) < 0.5f && std::abs(uses[v].ey - uses[u].ey) < 0.5f)
                    { group.push_back(v); done[v] = true; }
            std::stable_sort(group.begin(), group.end(),
                             [&](size_t l, size_t r) { return uses[l].otherY < uses[r].otherY; });
            const float n = (float)group.size();
            for (size_t j = 0; j < group.size(); ++j) {
                const auto& g = uses[group[j]];
                const float y = g.ey + ((float)j - (n - 1.0f) * 0.5f) * cfg.anchorSpread;
                memberEnd[g.slot] = { g.ex, juce::jlimit(g.tile.getY() + 1.0f, g.tile.getBottom() - 1.0f, y) };
            }
        }
    }

    // HEIGHTS. Arcs still nest by rank (36 + r*22 above their lower anchor), but r
    // now counts CROSS-column links only, and a rise that would carry the curve --
    // or the label riding on it -- out of the top of the visible strip is scaled
    // down instead of being clipped (R2). With every link fitting, as in a plain
    // unstacked strip, nothing is scaled and the arcs are the reference arcs.
    //
    // DIRECTION (2026-11-23). A link with at least one STACKED end bulges DOWN when
    // its two anchors average below the strip's vertical centre, UP otherwise (an
    // exact tie: in.tieBulgesDown, fixed per link). Links between unstacked blocks
    // always bulge up, exactly as before. Each direction nests and scales on its
    // own, against its own edge of the strip, so a pure-unstacked strip is laid out
    // exactly as before (every link UP, same ranks, same scale).
    std::vector<bool> down(links.size(), false);
    int nUp = 0, nDown = 0;
    for (size_t i = 0; i < links.size(); ++i) {
        const auto& in = links[i];
        if (!in.a.valid || !in.b.valid || isSameColumn(in.a.x, in.b.x)) continue;
        if (in.a.stacked || in.b.stacked) {
            const float ya = isMember(in.a) ? memberEnd[2 * i].y     : cfg.cy;
            const float yb = isMember(in.b) ? memberEnd[2 * i + 1].y : cfg.cy;
            const float avg = (ya + yb) * 0.5f;
            down[i] = avg > cfg.cy || (avg == cfg.cy && in.tieBulgesDown);
        }
        ++(down[i] ? nDown : nUp);
    }
    auto maxRiseFor = [&](int n) { return cfg.arcHBase + (float)juce::jmax(0, n - 1) * cfg.arcHStep; };
    const float topFloor    = vis.getY()      + cfg.margin + H * 0.5f + 1.0f;
    const float bottomFloor = vis.getBottom() - cfg.margin - H * 0.5f - 1.0f;
    int rankUp = 0, rankDown = 0;

    for (size_t i = 0; i < links.size(); ++i) {
        const auto& in = links[i];
        auto& p = out[i];
        if (!in.a.valid || !in.b.valid) continue;
        if (isSameColumn(in.a.x, in.b.x)) continue;     // pass 1 placed it
        p.visible = true;
        p.sameColumn = false;

        const float boxW = juce::jmax(in.labelW, in.pillW) + 4.0f;
        // A stacked end with no tile rect has nothing true to anchor to: draw
        // nothing rather than fall back to the column centre (2026-11-23).
        if ((in.a.stacked && in.a.tile.isEmpty()) || (in.b.stacked && in.b.tile.isEmpty())) {
            p.visible = false; p.labelVisible = false; continue;
        }
        const auto e1 = isMember(in.a) ? memberEnd[2 * i]     : endpoint(in.a);
        const auto e2 = isMember(in.b) ? memberEnd[2 * i + 1] : endpoint(in.b);
        p.anchorX1 = e1.x; p.anchorY1 = e1.y;
        p.anchorX2 = e2.x; p.anchorY2 = e2.y;
        {
            const float push = juce::jmin(cfg.edgeCtrlMax, cfg.edgeCtrlFrac * std::abs(e2.x - e1.x));
            const float dir  = (e2.x >= e1.x) ? 1.0f : -1.0f;
            p.ctrlX1 = e1.x + ((in.a.stacked && !in.a.tile.isEmpty()) ?  dir * push : 0.0f);
            p.ctrlX2 = e2.x + ((in.b.stacked && !in.b.tile.isEmpty()) ? -dir * push : 0.0f);
        }

        const bool  dn    = down[i];
        const float base  = dn ? juce::jmax(e1.y, e2.y) : juce::jmin(e1.y, e2.y);
        const float rise  = cfg.arcHBase + (float)(dn ? rankDown++ : rankUp++) * cfg.arcHStep;
        const float room  = dn ? bottomFloor - base : base - topFloor;
        const float scale = juce::jlimit(0.0f, 1.0f, room / juce::jmax(1.0f, maxRiseFor(dn ? nDown : nUp)));
        p.arcControlY = base + (dn ? 1.0f : -1.0f) * rise * scale;
        const auto mid = crossCurvePoint(p, 0.5f);
        p.midX = mid.x; p.midY = mid.y;

        // Cull a link whose arc has scrolled out of view (T67 rule, unchanged).
        const juce::Rectangle<float> arcBounds = juce::Rectangle<float>::leftTopRightBottom(
            juce::jmin(p.anchorX1, p.anchorX2), juce::jmin(p.arcControlY, p.anchorY1, p.anchorY2),
            juce::jmax(p.anchorX1, p.anchorX2), juce::jmax(p.arcControlY, p.anchorY1, p.anchorY2));
        if (!arcBounds.intersects(vis)) {
            p.labelVisible = false; p.offViewport = true; p.labelBox = {};
            continue;
        }
        const auto arcVis = arcBounds.getIntersection(vis);
        const bool arcRunsOff = arcBounds.getX() < vis.getX() || arcBounds.getRight() > vis.getRight();

        // LABEL. Candidates, cheapest first: (1) centred ON the curve, sliding
        // from t=0.5 out to tMin/tMax; (2) only then the old displacement search
        // around the midpoint, which costs extra so a label slides along its own
        // curve before it leaves it. Label-vs-label clearance (now with a vertical
        // gap too, since the plate overhangs its box) stays HARD; tiles stay a
        // priced preference exactly as before.
        struct Cand { float cost; juce::Rectangle<float> box; };
        std::vector<Cand> cands;
        const int nT = (int)std::round((cfg.tMax - cfg.tMin) / cfg.tStep);
        for (int k = 0; k <= nT; ++k) {
            const int   j = (k + 1) / 2 * ((k % 2) ? -1 : 1);       // 0,-1,+1,-2,+2...
            const float t = 0.5f + (float)j * cfg.tStep;
            if (t < cfg.tMin - 1e-4f || t > cfg.tMax + 1e-4f) continue;
            const auto c = crossCurvePoint(p, t);
            cands.push_back({ std::abs(t - 0.5f) * 20.0f,
                              clampBox({ c.x - boxW * 0.5f, c.y - H * 0.5f, boxW, H }, arcVis, arcRunsOff) });
        }
        {
            const int slideMax = (int)std::ceil(cfg.width  / juce::jmax(1.0f, cfg.slideStep));
            const int pushMax  = (int)std::ceil(cfg.height / juce::jmax(1.0f, cfg.pushStep));
            const int upMax    = juce::jmax(cfg.maxPush, pushMax);
            const juce::Rectangle<float> home(mid.x - boxW * 0.5f, mid.y - H * 0.5f, boxW, H);
            for (int sy = -upMax; sy <= pushMax; ++sy) {
                const float dy = (float)sy * cfg.pushStep;
                const float dyCost = std::abs(dy) * (sy <= 0 ? 1.0f : 1.25f);
                for (int sx = 0; sx <= slideMax; ++sx) {
                    const float dx = (float)sx * cfg.slideStep;
                    if (sx == 0 && sy == 0) continue;            // that is the t=0.5 candidate
                    const float cost = 10.0f + dyCost + dx * 0.5f;
                    cands.push_back({ cost, clampBox(home.translated( dx, dy), arcVis, arcRunsOff) });
                    if (sx > 0)
                        cands.push_back({ cost, clampBox(home.translated(-dx, dy), arcVis, arcRunsOff) });
                }
            }
        }
        std::stable_sort(cands.begin(), cands.end(),
                         [](const Cand& l, const Cand& r) { return l.cost < r.cost; });

        auto freeWithGap = [&](const juce::Rectangle<float>& r) {
            for (auto& c : committed)
                if (r.getRight()  + cfg.slotGap   > c.getX() && c.getRight()  + cfg.slotGap   > r.getX()
                 && r.getBottom() + cfg.labelVGap > c.getY() && c.getBottom() + cfg.labelVGap > r.getY())
                    return false;
            return true;
        };
        juce::Rectangle<float> box;
        bool settled = false; float bestCost = 0.0f;
        for (const auto& c : cands) {
            if (settled && c.cost >= bestCost) break;
            if (!freeWithGap(c.box)) continue;
            const float total = c.cost + (clearOfTiles(c.box) ? 0.0f : cfg.tilePenalty);
            if (settled && total >= bestCost) continue;
            box = c.box; bestCost = total; settled = true;     // only ever a candidate that PASSED
        }
        if (!settled) {
            p.labelVisible = false; p.degraded = true; p.labelBox = {};
            continue;
        }
        committed.push_back(box);
        p.labelVisible = true;
        p.displaced    = distanceToCrossCurve(p, box.getCentre()) > cfg.onCurveTol;
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
