#include "BlockLinkOverlay.h"
#include "LinkArcLayout.h"
#include "LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

void BlockLinkOverlay::setBlockAnchors(const juce::HashMap<juce::String, juce::Rectangle<int>>& bounds) {
    blockBounds.clear();
    for (auto it = bounds.begin(); it != bounds.end(); ++it)
        blockBounds.set(it.getKey(), it.getValue());
    repaint();
}

void BlockLinkOverlay::setLinkingSourceX(int x) {
    if (linkingSourceX == x) return;
    linkingSourceX = x;
    repaint();
}

void BlockLinkOverlay::paint(juce::Graphics& g) {
    if (!project) return;

    auto labelFont = LookAndFeel_BlockShuffler::uiFont(10.0f);
    auto probFont  = LookAndFeel_BlockShuffler::monoFont(10.0f);

    LinkArcLayout::Config cfg;
    cfg.width  = (float)getWidth();
    cfg.height = (float)getHeight();
    cfg.cy     = (float)(getHeight() / 2);

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

        juce::String labelText = nameA + " <-> " + nameB;
        juce::String probText  = juce::String((int)(link->swapProbability * 100)) + "%";

        in.labelW = LookAndFeel_BlockShuffler::measureTextWidth(labelFont, labelText) + 10.0f;
        in.pillW  = LookAndFeel_BlockShuffler::measureTextWidth(probFont,  probText)  + 12.0f;

        ins.push_back(in);
        labelTexts.add(labelText);
        probTexts.add(probText);
    }

    const auto placed = LinkArcLayout::layout(ins, cfg, reserved);

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
            // Leader from the bow apex up to the label it belongs to.
            g.setColour(col.withMultipliedAlpha(0.65f));
            g.drawLine(p.apexX, p.apexY,
                       p.labelBox.getCentreX(), p.labelBox.getBottom() + 2.0f, 1.0f);
        } else {
            arc.startNewSubPath(p.anchorX1, p.anchorY1);
            arc.cubicTo(p.anchorX1, p.arcControlY, p.anchorX2, p.arcControlY,
                        p.anchorX2, p.anchorY2);
            g.strokePath(arc, juce::PathStrokeType(2.0f,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded));
        }

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
