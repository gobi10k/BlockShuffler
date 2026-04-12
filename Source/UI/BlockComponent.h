#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Block.h"

namespace BlockShuffler {

/**
 * A single block tile in the BlockStrip.
 * Right-click: context menu (rename, color, done, delete, link, stack).
 * Double-click: inline rename.
 * Drag: reorder via DragAndDropContainer.
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
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    void setSelected(bool s);
    void setHighlighted(bool h);  ///< Pending link/stack target highlight
    void setPlaying(bool p);      ///< Currently playing in the arrangement
    void setDragTarget(bool d);   ///< Block is the drop target during a drag-to-stack
    void setDragTargetMode(bool isReorder);  ///< true = reorder within stack, false = stack
    bool isSelected()   const { return selected; }
    Block* getBlock()         { return block.get(); }

    // DragAndDropTarget — accepts clip drops from ClipRowComponent
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;

private:
    juce::WeakReference<Block> block;
    bool selected         = false;
    bool highlighted      = false;
    bool playing          = false;
    bool dragTarget       = false;
    bool dragTargetIsReorder  = false;  ///< true = reorder within stack, false = stack
    bool clipDropHighlight    = false;  ///< true = a clip is being dragged over this block

    std::function<void(Block*)>              onSelected;
    std::function<void(const juce::String&)> onDeleteRequested;
    std::function<void()>                    onMutated;          ///< visual-only repaint
    std::function<void(const juce::String&)> onLinkRequested;
    std::function<void(const juce::String&)> onStackRequested;

public:
    /// Called when a clip is dropped onto this block: (clipId, targetBlockId)
    std::function<void(const juce::String&, const juce::String&)> onClipDropped;

    /// Called just before a context-menu action changes the model — returns a project snapshot.
    std::function<juce::var()>             onCaptureSnapshot;
    /// Called after a context-menu mutation, with the pre-change snapshot, to record undo.
    std::function<void(const juce::var&)>  onUndoableMutation;
    std::function<void(const juce::String&)> onRemoveLinksRequested;
    /// Called when "Play from Here" is chosen — passes the block id.
    std::function<void(const juce::String&)> onPlayFromHereRequested;

private:

    juce::Label  nameLabel;
    juce::var    namePre;          // project snapshot taken when the name editor opens
    juce::String nameBeforeEdit;   // block.name value at editor-open time

    void showContextMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlockComponent)
};

} // namespace BlockShuffler
