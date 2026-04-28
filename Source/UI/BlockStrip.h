#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Project.h"
#include "BlockComponent.h"
#include "BlockLinkOverlay.h"
#include "LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

/**
 * Horizontally scrollable strip of block tiles.
 * Manages selection, link/stack "pending" mode, and drag-to-reorder/stack.
 *
 * Block dragging uses ComponentDragger (not DragAndDropContainer).
 * BlockComponent physically follows the mouse and calls onDragMoved / onDragEnded.
 * BlockStrip is the sole handler for block drop logic.
 *
 * Clip dragging still uses DragAndDropContainer; BlockComponent handles those drops.
 */
class BlockStrip : public juce::Component,
                   public juce::ChangeListener {
public:
    BlockStrip() = default;
    ~BlockStrip() override;

    /** Call after construction, before the component is shown. */
    void init(Project& project, BlockLinkOverlay* overlay = nullptr);

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    std::function<void(Block*)>              onBlockSelected;
    std::function<void(const juce::String&)> onPlayFromHereRequested;
    /// Called when "Play Block" is chosen — plays only that block in isolation.
    std::function<void(const juce::String&)> onPlayBlockRequested;
    /// Called when a clip is dragged and dropped onto a block: (clipId, targetBlockId)
    std::function<void(const juce::String&, const juce::String&)> onClipDropped;

    /** Programmatic selection (e.g. on startup or project load). */
    void selectBlock(Block* block);

    /** Highlight the currently-playing block during playback. Pass {} to clear. */
    void setPlayingBlock(const juce::String& blockId);

    /** Returns the block whose tile contains the given point (in BlockStrip-local coords),
     *  or nullptr if the point is outside all block tiles. */
    Block* getBlockAtLocalPoint(juce::Point<int> localPt) const;

    /** Cancels any pending link/stack mode (e.g. when Esc is pressed). */
    void cancelPendingMode();

    // ── Block drag callbacks — called by BlockComponent ──────────────────────────
    /** Called every frame during a block drag with the dragged component and its
     *  current centre in contentArea-local coordinates. Updates drop feedback.
     *  @param isShiftDrag  True when Shift was held at drag start (stack-move mode). */
    void updateDragFeedback(BlockComponent* draggedComp, juce::Point<int> centre, bool isShiftDrag = false);

    /** Called when a block drag ends. Processes the drop and rebuilds layout.
     *  @param isShiftDrag  True when Shift was held at drag start (stack-move mode). */
    void blockDropped(BlockComponent* draggedComp, juce::Point<int> centre, bool isShiftDrag = false);

    /** Clears all drop-feedback state and repaints. */
    void clearDragFeedback();

private:
    Project*          project  = nullptr;
    BlockLinkOverlay* overlay  = nullptr;

    // Custom Viewport subclass to detect scroll position changes
    struct ScrollNotifyViewport : public juce::Viewport {
        std::function<void()> onScrollChanged;
        void visibleAreaChanged(const juce::Rectangle<int>&) override {
            if (onScrollChanged) onScrollChanged();
        }
        void paint(juce::Graphics& g) override {
            g.fillAll(juce::Colour(LookAndFeel_BlockShuffler::bgMedium));
        }
    };
    ScrollNotifyViewport viewport;
    juce::Component  contentArea;
    juce::TextButton addButton { "+" };
    juce::Label      modeLabel;   ///< overlay shown during link/stack pending mode

    juce::OwnedArray<BlockComponent> blockComponents;
    juce::String selectedBlockId;
    juce::String playingBlockId;

    // Pending interaction modes (link / stack via context menu)
    enum class PendingMode { None, Link, Stack };
    PendingMode    pendingMode     = PendingMode::None;
    juce::String   pendingBlockId;

    static constexpr int blockW   = 100;
    static constexpr int blockH   = 120;
    static constexpr int blockGap =  10;  // wide enough for reliable gap-drop targeting
    static constexpr int padding  =   8;
    static constexpr int addBtnW  =  36;

    // Cache of contentArea-relative centre-X for each block (indexed parallel to project->blocks).
    // Used for scroll-into-view calculations.
    juce::Array<int> blockCentreXCache;

    // Static bounds of each block component as laid out by resized().
    // These don't change while a drag is in progress, so hit-testing against them
    // is consistent regardless of where the dragged tile has moved.
    juce::Array<juce::Rectangle<int>> originalBounds;

    // ── Drag-drop feedback state ──────────────────────────────────────────────────
    enum class DropAction { None, Reorder, Stack, RearrangeInStack };

    DropAction       currentDropAction = DropAction::None;
    int              dropTargetIndex   = -1;      ///< block index for Stack/Rearrange; insert-before for Reorder
    BlockComponent*  dropTargetComp    = nullptr; ///< target component for Stack / RearrangeInStack
    bool             isStackMove       = false;   ///< true when Shift+drag is moving a whole stack

    // Sibling positions recorded at the start of a Shift+stack drag.
    // Keys are valid only while no rebuild is in progress.
    juce::HashMap<BlockComponent*, juce::Point<int>> stackDragStartPositions;

    void beginStackDrag(int stackGroup);
    void moveStackComponents(int stackGroup, BlockComponent* draggedComp, juce::Point<int> delta);

    /// Non-null while a block is being dragged. Guards against rebuilding the
    /// component array while the drag event handler is still on the call stack.
    BlockComponent* activeDragComp = nullptr;

    /// Set by changeListenerCallback if a rebuild is requested during an active drag.
    /// Applied after the drag completes in blockDropped().
    bool needsRebuildAfterDrag = false;

    void rebuildBlocks();
    void deleteBlock(const juce::String& blockId);
    void enterLinkMode(const juce::String& fromBlockId);
    void enterStackMode(const juce::String& fromBlockId);
    void completePendingMode(const juce::String& targetBlockId);
    void updateOverlay();
    int  blockCentreX(int blockIndex) const;

    /** Converts a BlockStrip-local point to contentArea-local coordinates. */
    juce::Point<int> toContentPos(juce::Point<int> stripLocal) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlockStrip)
};

} // namespace BlockShuffler
