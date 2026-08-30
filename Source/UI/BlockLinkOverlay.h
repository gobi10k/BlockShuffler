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
     * @param bounds  Map of blockId → the tile's RECT in overlay-local coordinates.
     *
     * Full rects rather than just a centre X (2026-08-22): a link between two
     * members of one stack has identical centre X values, so the arc needs each
     * endpoint's own centre Y to bow out to the side of the column instead of
     * degenerating into a vertical line, and the label pass needs the tile rects
     * to keep labels off the block names.
     */
    void setBlockAnchors(const juce::HashMap<juce::String, juce::Rectangle<int>>& bounds);

    /**
     * The part of the overlay the user can actually see -- the strip's viewport,
     * in overlay-local coordinates. The overlay covers the WHOLE strip (including
     * the "+" button gutter) while the tiles scroll underneath it, so without this
     * the layout has no way to tell a link that has scrolled away from one that is
     * simply near the edge. Left unset, everything counts as visible.
     */
    void setViewportRect(juce::Rectangle<int> r);

    /** Show a "linking mode" indicator at this x position (-1 = off). */
    void setLinkingSourceX(int x);

private:
    Project* project = nullptr;
    juce::HashMap<juce::String, juce::Rectangle<int>> blockBounds;
    juce::Rectangle<int> viewportRect;
    int linkingSourceX = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlockLinkOverlay)
};

} // namespace BlockShuffler
