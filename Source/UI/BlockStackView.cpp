#include "BlockStackView.h"
#include "LookAndFeel_BlockShuffler.h"
namespace BlockShuffler {
void BlockStackView::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(LookAndFeel_BlockShuffler::bgMedium));
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::gridLineColor));
    g.drawRect(getLocalBounds());
    g.drawText("BlockStackView", getLocalBounds(), juce::Justification::centred);
}
void BlockStackView::resized() {}
} // namespace BlockShuffler
