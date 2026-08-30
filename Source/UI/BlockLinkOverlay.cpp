#include "BlockLinkOverlay.h"
#include "LinkArcLayout.h"
#include "LinkLabelMetrics.h"
#include "LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

void BlockLinkOverlay::setBlockAnchors(const juce::HashMap<juce::String, juce::Rectangle<int>>& bounds) {
    blockBounds.clear();
    for (auto it = bounds.begin(); it != bounds.end(); ++it)
        blockBounds.set(it.getKey(), it.getValue());
    repaint();
}

void BlockLinkOverlay::setViewportRect(juce::Rectangle<int> r) {
    if (viewportRect == r) return;
    viewportRect = r;
    repaint();
}

void BlockLinkOverlay::setLinkingSourceX(int x) {
    if (linkingSourceX == x) return;
    linkingSourceX = x;
    repaint();
}

void BlockLinkOverlay::paint(juce::Graphics& g) {
    if (!project) return;

    // These two Font objects are used for BOTH the measurement that sizes the
    // collision box and the g.setFont that draws the glyphs. Keep it that way.
    const auto labelFont = LinkLabelMetrics::nameFont();
    const auto probFont  = LinkLabelMetrics::pillFont();

    LinkArcLayout::Config cfg;
    cfg.width  = (float)getWidth();
    cfg.height = (float)getHeight();
    cfg.cy     = (float)(getHeight() / 2);
    // Empty until BlockStrip tells us otherwise, which the layout reads as
    // "the whole strip is visible".
    cfg.viewport = viewportRect.toFloat();

    // Collect the inputs the pure layout pass needs, and the tile rects labels
    // must keep clear of so a link label never lands on a block's name.
    std::vector<LinkArcLayout::LinkIn> ins;
    std::vector<juce::Rectangle<float>> reserved;
    juce::StringArray labelTexts, probTexts;

    for (auto it = blockBounds.begin(); it != blockBounds.end(); ++it)
        reserved.push_back(it.getValue().toFloat());

    auto anchorFor = [&](const juce::String& id) {
        LinkArcLayout::Anchor a;
        if (blockBounds.contains(id)) {
            auto r = blockBounds[id].toFloat();
            a.x = r.getCentreX();
            a.y = r.getCentreY();
            a.valid = true;
            cfg.colHalfW = r.getWidth() * 0.5f;   // tiles are uniform width
        }
        return a;
    };

    for (auto* link : project->links) {
        LinkArcLayout::LinkIn in;
        in.a = anchorFor(link->blockA);
        in.b = anchorFor(link->blockB);

        juce::String nameA = "?", nameB = "?";
        if (auto* ba = project->getBlockById(link->blockA)) nameA = ba->name;
        if (auto* bb = project->getBlockById(link->blockB)) nameB = bb->name;

        const juce::String labelText = LinkLabelMetrics::nameText(nameA, nameB);
        const juce::String probText  = LinkLabelMetrics::pillText(link->swapProbability);

        in.labelW = LinkLabelMetrics::nameWidth(labelFont, labelText);
        in.pillW  = LinkLabelMetrics::pillWidth(probFont,  probText);

        ins.push_back(in);
        labelTexts.add(labelText);
        probTexts.add(probText);
    }

    const auto placed = LinkArcLayout::layout(ins, cfg, reserved);

    // Everything belonging to a link is clipped to the viewport. A link whose arc
    // has scrolled out of view is culled by the layout above; one that is only
    // PARTLY out keeps its natural place on its arc and is cut off at the edge
    // here, rather than being shoved inward to stay whole.
    {
    juce::Graphics::ScopedSaveState clipToViewport(g);
    if (!viewportRect.isEmpty()) g.reduceClipRegion(viewportRect);

    int linkIndex = 0;
    for (auto* link : project->links) {
        const auto& p = placed[(size_t)linkIndex];
        if (!p.visible) { ++linkIndex; continue; }

        auto col = juce::Colour(LookAndFeel_BlockShuffler::accentCol)
                       .withAlpha(0.4f + 0.6f * link->swapProbability);
        g.setColour(col);

        juce::Path arc;
        if (p.sameColumn) {
            // Both endpoints are members of one stack: bracket OUT to the side of
            // the column instead of drawing a straight line down through the tiles.
            arc.startNewSubPath(p.anchorX1, p.anchorY1);
            arc.cubicTo(p.apexX, p.anchorY1, p.apexX, p.anchorY2, p.anchorX2, p.anchorY2);
            g.strokePath(arc, juce::PathStrokeType(2.0f,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded));
            // The label normally sits right beside the apex, so it needs no
            // leader. One is drawn only when a collision moved it off that spot.
            if (p.labelVisible && p.displaced) {
                const juce::Point<float> apex(p.apexX, p.apexY);
                g.setColour(col.withMultipliedAlpha(0.65f));
                g.drawLine({ apex, p.labelBox.getConstrainedPoint(apex) }, 1.0f);
            }
        } else {
            arc.startNewSubPath(p.anchorX1, p.anchorY1);
            arc.cubicTo(p.anchorX1, p.arcControlY, p.anchorX2, p.arcControlY,
                        p.anchorX2, p.anchorY2);
            g.strokePath(arc, juce::PathStrokeType(2.0f,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded));

            // A label sits on its own arc by default. Only one that a collision
            // actually displaced gets a leader tying it back to its arc.
            if (p.labelVisible && p.displaced) {
                const juce::Point<float> mid((p.anchorX1 + p.anchorX2) * 0.5f, p.arcControlY);
                g.setColour(col.withMultipliedAlpha(0.65f));
                g.drawLine({ mid, p.labelBox.getConstrainedPoint(mid) }, 1.0f);
            }
        }

        // The label is skipped only when the strip had no room for it anywhere
        // without covering another label; the arc above is still drawn.
        if (!p.labelVisible) { ++linkIndex; continue; }

        // Name row — backed so it stays readable over an arc passing behind it.
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::bgDark).withAlpha(0.82f));
        g.fillRoundedRectangle(p.nameRect.expanded(2.0f, 1.0f), 3.0f);
        g.setColour(col.withAlpha(1.0f));
        g.setFont(labelFont);
        g.drawText(labelTexts[linkIndex], p.nameRect, juce::Justification::centred);

        // Probability pill
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::bgLight).withAlpha(0.88f));
        g.fillRoundedRectangle(p.pillRect, 5.5f);
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol));
        g.setFont(probFont);
        g.drawText(probTexts[linkIndex], p.pillRect, juce::Justification::centred);

        ++linkIndex;
    }
    }   // end viewport clip -- the linking-mode indicator below is deliberately
        // NOT clipped: it marks the whole strip's source column, not one arc.

    // Linking-mode indicator: the vertical line marking the SOURCE block only.
    // The instruction text used to be drawn here too — it is now owned solely by
    // BlockStrip::modeLabel, which also covers stack mode and the Esc hint. Two
    // centred strings at the top of the same rectangle rendered on top of each
    // other (Carter 2026-08-22 screenshot).
    if (linkingSourceX >= 0) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.8f));
        g.drawLine((float)linkingSourceX, 0.0f,
                   (float)linkingSourceX, (float)getHeight(), 2.0f);
    }
}

} // namespace BlockShuffler
