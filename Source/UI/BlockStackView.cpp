#include "BlockStackView.h"
namespace BlockShuffler {
void BlockStackView::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF2D2D2D));
    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds());
    g.drawText("BlockStackView", getLocalBounds(), juce::Justification::centred);
}
void BlockStackView::resized() {}
} // namespace BlockShuffler
