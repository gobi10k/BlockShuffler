#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Project.h"

namespace BlockShuffler {

/**
 * Transparent overlay drawn on top of the BlockStrip.
 * Draws curved arcs between linked blocks and a "linking mode" indicator.
 *
 * Call setProject() + setBlockPositions() whenever the layout changes.
 */
class BlockLinkOverlay : public juce::Component {
public:
    BlockLinkOverlay() = default;
    ~BlockLinkOverlay() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override {}

    void setProject(Project* proj) { project = proj; }

    /**
     * Called by BlockStrip after layout so the overlay knows where to draw arcs.
     * @param positions  Map of blockId → centreX in overlay-local coordinates.
     */
    void setBlockPositions(const juce::HashMap<juce::String, int>& positions);

    /** Show a "linking mode" indicator at this x position (-1 = off). */
    void setLinkingSourceX(int x);

private:
    Project* project = nullptr;
    juce::HashMap<juce::String, int> blockCentreX;
    int linkingSourceX = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlockLinkOverlay)
};

} // namespace BlockShuffler
