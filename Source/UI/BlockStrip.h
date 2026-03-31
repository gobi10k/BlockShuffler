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

    std::function<void(Block*)> onBlockSelected;

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
    void itemDragEnter(const SourceDetails&) override {}
    void itemDragExit(const SourceDetails&) override {}
    void itemDragMove(const SourceDetails&) override {}

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
    static constexpr int blockGap =   6;
    static constexpr int padding  =   8;
    static constexpr int addBtnW  =  36;

    // Cache of contentArea-relative centre-X for each block (indexed parallel to project->blocks)
    juce::Array<int> blockCentreXCache;

    void rebuildBlocks();
    void deleteBlock(const juce::String& blockId);
    void enterLinkMode(const juce::String& fromBlockId);
    void enterStackMode(const juce::String& fromBlockId);
    void completePendingMode(const juce::String& targetBlockId);
    void updateOverlay();
    int  blockCentreX(int blockIndex) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlockStrip)
};

} // namespace BlockShuffler
