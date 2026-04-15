#include "ClipListPanel.h"
namespace BlockShuffler {
void ClipListPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF2D2D2D));
    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds());
    g.drawText("ClipListPanel", getLocalBounds(), juce::Justification::centred);
}
void ClipListPanel::resized() {}
} // namespace BlockShuffler
