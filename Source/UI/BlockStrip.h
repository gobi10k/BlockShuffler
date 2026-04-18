#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Project.h"
#include "BlockComponent.h"
#include "BlockLinkOverlay.h"

namespace BlockShuffler {

/**
 * Horizontally scrollable strip of block tiles.
 * Manages selection, link/stack "pending" mode, and drag-to-reorder.
 */
class BlockStrip : public juce::Component,
                   public juce::ChangeListener,
                   public juce::DragAndDropTarget {
public:
    BlockStrip() = default;
    ~BlockStrip() override;

    /** Call after construction, before the component is shown. */
    void init(Project& project, BlockLinkOverlay* overlay = nullptr);

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    std::function<void(Block*)>              onBlockSelected;
    std::function<void(const juce::String&)> onPlayFromHereRequested;

    /** Programmatic selection (e.g. on startup or project load). */
    void selectBlock(Block* block);

    /** Highlight the currently-playing block during playback. Pass {} to clear. */
    void setPlayingBlock(const juce::String& blockId);

    /** Returns the block whose tile contains the given point (in BlockStrip-local coords),
     *  or nullptr if the point is outside all block tiles. */
    Block* getBlockAtLocalPoint(juce::Point<int> localPt) const;

    /** Cancels any pending link/stack mode (e.g. when Esc is pressed). */
    void cancelPendingMode();

    // DragAndDropTarget
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails& details) override;

private:
    Project*          project  = nullptr;
    BlockLinkOverlay* overlay  = nullptr;

    // Custom Viewport subclass to detect scroll position changes
    struct ScrollNotifyViewport : public juce::Viewport {
        std::function<void()> onScrollChanged;
        void visibleAreaChanged(const juce::Rectangle<int>&) override {
            if (onScrollChanged) onScrollChanged();
        }
    };
    ScrollNotifyViewport viewport;
    juce::Component  contentArea;
    juce::TextButton addButton { "+" };
    juce::Label      modeLabel;   ///< overlay shown during link/stack pending mode

    juce::OwnedArray<BlockComponent> blockComponents;
    juce::String selectedBlockId;
    juce::String playingBlockId;

    // Pending interaction modes
    enum class PendingMode { None, Link, Stack };
    PendingMode    pendingMode     = PendingMode::None;
    juce::String   pendingBlockId;  ///< Source block for the pending link/stack

    static constexpr int blockW   = 100;
    static constexpr int blockH   = 120;
    static constexpr int blockGap =  16;
    static constexpr int padding  =   8;
    static constexpr int addBtnW  =  36;

    // Cache of contentArea-relative centre-X for each block (indexed parallel to project->blocks)
    juce::Array<int> blockCentreXCache;

    // Index (into blockComponents) of the block currently under a drag-to-stack hover; -1 = none
    int dragOverIndex = -1;

    // Drag state tracking for visual feedback
    int  dragSourceIndex = -1;
    int  dragSourceSlot  = -1;
    bool dragIsUnstacking = false;  // true = moving to different slot (will unstack)
    int  dragDropSlot = -1;         // horizontal slot where drop will occur

    void rebuildBlocks();
    void deleteBlock(const juce::String& blockId);
    void enterLinkMode(const juce::String& fromBlockId);
    void enterStackMode(const juce::String& fromBlockId);
    void completePendingMode(const juce::String& targetBlockId);
    void updateOverlay();
    int  blockCentreX(int blockIndex) const;

    /** Returns the contentArea-relative X and Y for a strip-local drag position. */
    juce::Point<int> toContentPos(juce::Point<int> stripLocal) const;

    /** Returns the block index (into blockComponents/project->blocks) at a contentArea point,
     *  or -1 if the point is not inside any block tile. */
    int blockIndexAtContentPos(juce::Point<int> contentPos) const;

    /** Like blockIndexAtContentPos but only matches if the point falls inside the
     *  central 60% of the block horizontally. The outer 20% on each side counts as
     *  a reorder zone (treated the same as the gap between blocks). */
    int blockStackZoneAtContentPos(juce::Point<int> contentPos) const;

    /** Sets the drag-over highlight on the block at dragOverIndex, clearing the previous one. */
    void setDragOver(int newIndex, bool isReorder = false);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlockStrip)
};

} // namespace BlockShuffler
