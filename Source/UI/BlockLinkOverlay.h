#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Project.h"
#if BLOCKSHUFFLER_QA_HOOK
#include "LinkArcLayout.h"
#define BS_OVERLAY_HAS_QA 1
#endif

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

    /** Link mode: outline this block's OWN tile as the link source (empty = off).
     *  Keyed by block id, not an x position, so it marks the member tile rather
     *  than the whole stack column and follows the tile when the strip scrolls. */
    void setLinkingSource(const juce::String& blockId);

    /** Strip-local rect no link label may cover (the link/stack-mode banner);
     *  empty = none. */
    void setKeepOut(juce::Rectangle<int> r);

private:
    Project* project = nullptr;
    juce::HashMap<juce::String, juce::Rectangle<int>> blockBounds;
    juce::Rectangle<int> viewportRect;
    juce::String linkingSourceId;
    juce::Rectangle<int> keepOutRect;

#if BLOCKSHUFFLER_QA_HOOK
    // QA build only: a COPY of what the last paint() laid out, for the dump.
    friend struct QaHook;
    std::vector<LinkArcLayout::LinkIn> qaIns;
    std::vector<LinkArcLayout::Placed> qaPlaced;
    std::vector<float> qaArcAlpha, qaLabelAlpha;   // what paint() actually used
    int qaPaintSeq = 0;                             // which link was stroked in what order
    std::vector<int> qaDrawOrder;
#endif
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlockLinkOverlay)
};

} // namespace BlockShuffler
