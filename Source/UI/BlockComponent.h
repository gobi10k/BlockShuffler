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
class BlockComponent : public juce::Component {
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
    bool isSelected()   const { return selected; }
    Block* getBlock()         { return block.get(); }

private:
    juce::WeakReference<Block> block;
    bool selected    = false;
    bool highlighted = false;
    bool playing     = false;

    std::function<void(Block*)>              onSelected;
    std::function<void(const juce::String&)> onDeleteRequested;
    std::function<void()>                    onMutated;          ///< visual-only repaint
    std::function<void(const juce::String&)> onLinkRequested;
    std::function<void(const juce::String&)> onStackRequested;

public:
    /// Called just before a context-menu action changes the model — returns a project snapshot.
    std::function<juce::var()>             onCaptureSnapshot;
    /// Called after a context-menu mutation, with the pre-change snapshot, to record undo.
    std::function<void(const juce::var&)>  onUndoableMutation;
    std::function<void(const juce::String&)> onRemoveLinksRequested;

private:

    juce::Label  nameLabel;
    juce::var    namePre;          // project snapshot taken when the name editor opens
    juce::String nameBeforeEdit;   // block.name value at editor-open time

    void showContextMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlockComponent)
};

} // namespace BlockShuffler
