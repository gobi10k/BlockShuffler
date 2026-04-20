#include "ClipListPanel.h"
#include "LookAndFeel_BlockShuffler.h"
namespace BlockShuffler {
void ClipListPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(LookAndFeel_BlockShuffler::bgMedium));
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::gridLineColor));
    g.drawRect(getLocalBounds());
    g.drawText("ClipListPanel", getLocalBounds(), juce::Justification::centred);
}
void ClipListPanel::resized() {}
} // namespace BlockShuffler
