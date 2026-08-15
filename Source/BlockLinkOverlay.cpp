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

    const int cy    = getHeight() / 2;
    const int arcH  = 36;  // arc peak height above centre line

    // Draw arcs for all links
    for (auto* link : project->links) {
        bool hasA = blockCentreX.contains(link->blockA);
        bool hasB = blockCentreX.contains(link->blockB);
        if (!hasA || !hasB) continue;

        float x1 = (float)blockCentreX[link->blockA];
        float x2 = (float)blockCentreX[link->blockB];
        float midX = (x1 + x2) * 0.5f;
        float peakY = (float)(cy - arcH);

        juce::Path arc;
        arc.startNewSubPath(x1, (float)cy);
        arc.cubicTo(x1,    peakY, x2,    peakY, x2, (float)cy);

        // Colour: accent, opacity driven by swapProbability
        auto col = juce::Colour(LookAndFeel_BlockShuffler::accentCol)
                       .withAlpha(0.4f + 0.6f * link->swapProbability);
        g.setColour(col);
        g.strokePath(arc, juce::PathStrokeType(2.0f,
                          juce::PathStrokeType::curved,
                          juce::PathStrokeType::rounded));

        // Probability label at arc peak
        g.setFont(10.0f);
        g.setColour(col.withAlpha(1.0f));
        g.drawText(juce::String((int)(link->swapProbability * 100)) + "%",
                   juce::Rectangle<float>(midX - 16.0f, peakY - 14.0f, 32.0f, 14.0f),
                   juce::Justification::centred);
    }

    // Linking mode indicator
    if (linkingSourceX >= 0) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.8f));
        g.drawLine((float)linkingSourceX, 0.0f,
                   (float)linkingSourceX, (float)getHeight(), 2.0f);
        g.setFont(11.0f);
        g.drawText("Click another block to link",
                   getLocalBounds().removeFromTop(16),
                   juce::Justification::centred);
    }
}

} // namespace BlockShuffler
