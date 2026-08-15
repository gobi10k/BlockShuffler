#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Block.h"

namespace BlockShuffler {

/**
 * A single block tile in the BlockStrip.
 * Right-click: context menu (rename, color, done, delete, link, stack).
 * Double-click: inline rename.
 * Drag: uses ComponentDragger so the tile physically follows the mouse.
 *       BlockStrip receives onDragMoved / onDragEnded callbacks and handles
 *       the stack / reorder / rearrange-in-stack logic.
 *
 * DragAndDropTarget is kept only for "clip:" drags (from ClipRowComponent).
 */
class BlockComponent : public juce::Component,
                       public juce::DragAndDropTarget {
public:
    BlockComponent(Block& block,
                   std::function<void(Block*)>              onSelected,
                   std::function<void(const juce::String&)> onDeleteRequested,
                   std::function<void()>                    onMutated,
                   std::function<void(const juce::String&)> onLinkRequested,
                   std::function<void(const juce::String&)> onStackRequested);
    ~BlockComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    void setSelected(bool s);
    void setHighlighted(bool h);  ///< Pending link/stack target highlight
    void setPlaying(bool p);      ///< Currently playing in the arrangement
    bool isSelected()   const { return selected; }
    Block* getBlock()         { return block.get(); }

    // DragAndDropTarget — accepts clip drops only (from ClipRowComponent)
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;

private:
    juce::WeakReference<Block> block;
    bool selected          = false;
    bool highlighted       = false;
    bool playing           = false;
    bool clipDropHighlight = false;  ///< true = a clip drag is hovering over this block

    // ComponentDragger — block follows the mouse; parent strip handles the drop.
    juce::ComponentDragger   dragger;
    bool                     isDragging   = false;
    juce::Point<int>         dragStartPos;   ///< position in parent when mouseDown fired

    std::function<void(Block*)>              onSelected;
    std::function<void(const juce::String&)> onDeleteRequested;
    std::function<void()>                    onMutated;
    std::function<void(const juce::String&)> onLinkRequested;
    std::function<void(const juce::String&)> onStackRequested;

public:
    /// Called when a clip is dropped onto this block: (clipId, targetBlockId)
    std::function<void(const juce::String&, const juce::String&)> onClipDropped;

    /// Called just before a context-menu action changes the model — returns a project snapshot.
    std::function<juce::var()>               onCaptureSnapshot;
    /// Called after a context-menu mutation with the pre-change snapshot (triggers undo push).
    std::function<void(const juce::var&)>    onUndoableMutation;
    std::function<void(const juce::String&)> onRemoveLinksRequested;
    /// Called when "Play from Here" is chosen.
    std::function<void(const juce::String&)> onPlayFromHereRequested;

    /// Fired every time the block moves during a drag.
    /// @param comp   The BlockComponent being dragged.
    /// @param centre The component's current centre in its parent (contentArea) coordinates.
    std::function<void(BlockComponent*, juce::Point<int>)> onDragMoved;

    /// Fired when the mouse is released after a drag.
    /// @param comp   The BlockComponent that was dragged.
    /// @param centre The component's centre at release time, in parent (contentArea) coordinates.
    std::function<void(BlockComponent*, juce::Point<int>)> onDragEnded;

private:
    juce::Label  nameLabel;
    juce::var    namePre;         ///< project snapshot taken when the name editor opens
    juce::String nameBeforeEdit;  ///< block.name at editor-open time

    void showContextMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlockComponent)
};

} // namespace BlockShuffler
