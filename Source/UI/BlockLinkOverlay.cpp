#include "BlockLinkOverlay.h"
#include "LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

void BlockLinkOverlay::setBlockPositions(const juce::HashMap<juce::String, int>& positions) {
    blockCentreX.clear();
    for (auto it = positions.begin(); it != positions.end(); ++it)
        blockCentreX.set(it.getKey(), it.getValue());
    repaint();
}

void BlockLinkOverlay::setLinkingSourceX(int x) {
    if (linkingSourceX == x) return;
    linkingSourceX = x;
    repaint();
}

void BlockLinkOverlay::paint(juce::Graphics& g) {
    if (!project) return;

    const float cy       = (float)(getHeight() / 2);
    const float arcHBase = 36.0f;
    const float arcHStep = 22.0f;

    const float nameRowH    = 14.0f;
    const float pillH       = 15.0f;
    const float rowGap      =  2.0f;
    const float totalLabelH = nameRowH + rowGap + pillH;

    // Track placed label bounds so we can push new ones upward to avoid overlap.
    juce::Array<juce::Rectangle<float>> placedLabels;

    int linkIndex = 0;
    for (auto* link : project->links) {
        bool hasA = blockCentreX.contains(link->blockA);
        bool hasB = blockCentreX.contains(link->blockB);
        if (!hasA || !hasB) { ++linkIndex; continue; }

        float x1   = (float)blockCentreX[link->blockA];
        float x2   = (float)blockCentreX[link->blockB];
        float midX = (x1 + x2) * 0.5f;

        juce::String nameA = "?", nameB = "?";
        if (auto* ba = project->getBlockById(link->blockA)) nameA = ba->name;
        if (auto* bb = project->getBlockById(link->blockB)) nameB = bb->name;

        juce::String labelText = nameA + " <-> " + nameB;
        juce::String probText  = juce::String((int)(link->swapProbability * 100)) + "%";

        auto  labelFont = LookAndFeel_BlockShuffler::uiFont(10.0f);
        auto  probFont  = LookAndFeel_BlockShuffler::monoFont(10.0f);
        float labelW    = LookAndFeel_BlockShuffler::measureTextWidth(labelFont, labelText) + 10.0f;
        float pillW     = LookAndFeel_BlockShuffler::measureTextWidth(probFont,  probText)  + 12.0f;
        float boxW      = juce::jmax(labelW, pillW);

        // Start label at the arc peak for this link index, then resolve collisions.
        float arcH     = arcHBase + (float)linkIndex * arcHStep;
        float labelTop = cy - arcH - totalLabelH - 4.0f;

        juce::Rectangle<float> labelBox(midX - boxW * 0.5f, labelTop, boxW, totalLabelH);

        // Push upward (18 px per step) until no overlap with previously placed labels.
        for (int attempt = 0; attempt < 25; ++attempt) {
            bool collides = false;
            for (auto& placed : placedLabels) {
                if (labelBox.intersects(placed.expanded(2.0f, 2.0f))) {
                    labelBox.translate(0.0f, -18.0f);
                    collides = true;
                    break;
                }
            }
            if (!collides) break;
        }
        placedLabels.add(labelBox);

        // Arc control points anchor just below the label so the arc rises to meet it.
        float arcControlY = labelBox.getBottom() + 4.0f;

        auto col = juce::Colour(LookAndFeel_BlockShuffler::accentCol)
                       .withAlpha(0.4f + 0.6f * link->swapProbability);

        // Draw arc
        juce::Path arc;
        arc.startNewSubPath(x1, cy);
        arc.cubicTo(x1, arcControlY, x2, arcControlY, x2, cy);
        g.setColour(col);
        g.strokePath(arc, juce::PathStrokeType(2.0f,
                          juce::PathStrokeType::curved,
                          juce::PathStrokeType::rounded));

        // Name label row
        g.setColour(col.withAlpha(1.0f));
        g.setFont(labelFont);
        g.drawText(labelText,
                   juce::Rectangle<float>(midX - labelW * 0.5f, labelBox.getY(), labelW, nameRowH),
                   juce::Justification::centred);

        // Probability pill
        juce::Rectangle<float> pill(midX - pillW * 0.5f,
                                     labelBox.getY() + nameRowH + rowGap,
                                     pillW, pillH);
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::bgLight).withAlpha(0.88f));
        g.fillRoundedRectangle(pill, 5.5f);
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol));
        g.setFont(probFont);
        g.drawText(probText, pill, juce::Justification::centred);

        ++linkIndex;
    }

    // Linking mode indicator
    if (linkingSourceX >= 0) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.8f));
        g.drawLine((float)linkingSourceX, 0.0f,
                   (float)linkingSourceX, (float)getHeight(), 2.0f);
        g.setFont(LookAndFeel_BlockShuffler::uiFont(11.0f));
        g.drawText("Click another block to link",
                   getLocalBounds().removeFromTop(16),
                   juce::Justification::centred);
    }
}

} // namespace BlockShuffler
