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
//   LABELS: each label starts centred on its arc's anchor (mid-X, or the bow
//   apex for a same-column link) one "arc height" above mid-height, then is
//   pushed UP in fixed steps until it clears every already-placed label and
//   every reserved rect (the block tiles). If it runs out of headroom it is
//   shifted one lane sideways and retried, right lane first then left. The
//   final box is always clamped inside the strip bounds — that clamp wins over
//   collision avoidance, since a label outside the component is invisible.

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
    float arcHStep = 22.0f;

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

    juce::Rectangle<float> labelBox;   ///< name row + gap + pill, the collision unit
    juce::Rectangle<float> nameRect;
    juce::Rectangle<float> pillRect;
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
    std::vector<juce::Rectangle<float>> placedBoxes;

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
        if (r.getRight() > cfg.width - cfg.margin)  r.setX(cfg.width - cfg.margin - r.getWidth());
        if (r.getX() < cfg.margin)                  r.setX(cfg.margin);
        if (r.getBottom() > cfg.height - cfg.margin) r.setY(cfg.height - cfg.margin - r.getHeight());
        if (r.getY() < cfg.margin)                  r.setY(cfg.margin);
        return r;
    };

    auto collides = [&](const juce::Rectangle<float>& r) {
        for (auto& p : placedBoxes) if (r.intersects(p.expanded(2.0f, 2.0f))) return true;
        for (auto& p : reserved)    if (r.intersects(p))                      return true;
        return false;
    };

    for (size_t i = 0; i < links.size(); ++i) {
        const auto& in = links[i];
        auto& p = out[i];
        if (!in.a.valid || !in.b.valid) continue;
        p.visible = true;

        const float boxW = juce::jmax(in.labelW, in.pillW);
        p.sameColumn = isSameColumn(in.a.x, in.b.x);

        float labelAnchorX;
        if (p.sameColumn) {
            const float colX = (in.a.x + in.b.x) * 0.5f;
            const int   k    = nextColumnIndex(colX);
            const float bow  = cfg.colHalfW + cfg.bowBase + (float)k * cfg.bowStep;

            // Prefer the side with room; alternate by k so brackets never coincide.
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

        const float startTop = cfg.cy - (cfg.arcHBase + (float)i * cfg.arcHStep) - H - 4.0f;

        // Lane 0 = on the anchor, then one box-width right, then left.
        juce::Rectangle<float> box;
        bool settled = false;
        for (int lane = 0; lane < 3 && !settled; ++lane) {
            const float laneDx = (lane == 0) ? 0.0f
                               : (lane == 1) ?  (boxW + 8.0f)
                                             : -(boxW + 8.0f);
            juce::Rectangle<float> trial(labelAnchorX + laneDx - boxW * 0.5f,
                                         startTop, boxW, H);
            trial = clampBox(trial);
            for (int step = 0; step < cfg.maxPush; ++step) {
                if (!collides(trial)) { settled = true; break; }
                if (trial.getY() - cfg.pushStep < cfg.margin) break;   // out of headroom
                trial.translate(0.0f, -cfg.pushStep);
            }
            box = trial;
        }
        // The bounds clamp is absolute: a label outside the strip is invisible,
        // which is worse than one that still touches something.
        box = clampBox(box);
        placedBoxes.push_back(box);

        p.labelBox    = box;
        p.nameRect    = juce::Rectangle<float>(box.getCentreX() - in.labelW * 0.5f,
                                               box.getY(), in.labelW, cfg.nameRowH);
        p.pillRect    = juce::Rectangle<float>(box.getCentreX() - in.pillW * 0.5f,
                                               box.getY() + cfg.nameRowH + cfg.rowGap,
                                               in.pillW, cfg.pillH);
        p.arcControlY = box.getBottom() + 4.0f;
    }

    return out;
}

} // namespace LinkArcLayout
} // namespace BlockShuffler
