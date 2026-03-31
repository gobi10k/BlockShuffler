#include "BlockComponent.h"
#include "LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

BlockComponent::BlockComponent(Block& block_,
                               std::function<void(Block*)>              onSelected_,
                               std::function<void(const juce::String&)> onDeleteRequested_,
                               std::function<void()>                    onMutated_,
                               std::function<void(const juce::String&)> onLinkRequested_,
                               std::function<void(const juce::String&)> onStackRequested_)
    : block(block_),
      onSelected(std::move(onSelected_)),
      onDeleteRequested(std::move(onDeleteRequested_)),
      onMutated(std::move(onMutated_)),
      onLinkRequested(std::move(onLinkRequested_)),
      onStackRequested(std::move(onStackRequested_))
{
    nameLabel.setText(block.name, juce::dontSendNotification);
    nameLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    nameLabel.setColour(juce::Label::textColourId,
                        juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
    nameLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setEditable(false, true, false);
    nameLabel.onEditorShow = [this] {
        nameBeforeEdit = block.name;
        namePre = onCaptureSnapshot ? onCaptureSnapshot() : juce::var{};
    };
    nameLabel.onTextChange = [this] {
        block.name = nameLabel.getText();
        if (onMutated) onMutated();
    };
    nameLabel.onEditorHide = [this] {
        // In JUCE 8, onEditorHide fires before onTextChange, so block.name still
        // holds the old value here. Read the committed text directly from the label.
        juce::String newName = nameLabel.getText();
        if (newName != nameBeforeEdit && !namePre.isVoid()) {
            block.name = newName;  // update now so toJSON() inside recordMutation sees the new name
            if (onUndoableMutation)
                onUndoableMutation(namePre);
        }
        namePre = juce::var{};
    };
    // Don't intercept mouse events — let them fall through to BlockComponent
    // so right-click anywhere on the block opens the context menu.
    nameLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(nameLabel);
}

void BlockComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);

    // Base background
    auto bg = selected    ? juce::Colour(LookAndFeel_BlockShuffler::bgLight)
              : highlighted ? juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.25f)
                            : juce::Colour(LookAndFeel_BlockShuffler::bgMedium);
    if (block.isDone) bg = bg.withAlpha(0.45f);
    g.setColour(bg);
    g.fillRoundedRectangle(bounds, 5.0f);

    // Subtle block-color tint over the background so the color is visible everywhere
    if (!block.isDone)
        g.setColour(block.color.withAlpha(0.12f));
    else
        g.setColour(block.color.withAlpha(0.06f));
    g.fillRoundedRectangle(bounds, 5.0f);

    // Colored top bar (thicker) — green when playing
    g.setColour(playing ? juce::Colour(LookAndFeel_BlockShuffler::startMarkerCol) : block.color);
    g.fillRoundedRectangle(bounds.removeFromTop(playing ? 9.0f : 8.0f), 3.0f);

    // Border — accent if highlighted, block color otherwise
    auto borderCol = highlighted ? juce::Colour(LookAndFeel_BlockShuffler::accentCol)
                                 : block.color.withAlpha(selected ? 1.0f : 0.6f);
    g.setColour(borderCol);
    float borderW = (selected || highlighted) ? 2.0f : 1.0f;
    if (block.isOverlapping) {
        // Dashed border for overlapping blocks
        juce::Path solidPath;
        solidPath.addRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 5.0f);
        juce::Path dashedPath;
        float dashLengths[] = { 5.0f, 3.0f };
        juce::PathStrokeType(borderW).createDashedStroke(dashedPath, solidPath,
                                                          dashLengths, 2);
        g.fillPath(dashedPath);
    } else {
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 5.0f, borderW);
    }

    // Stack badge (top-right): show stack group number
    if (block.stackGroup >= 0) {
        auto badge = getLocalBounds().removeFromTop(16).removeFromRight(16);
        g.setColour(block.color);
        g.fillEllipse(badge.toFloat().reduced(1.0f));
        g.setColour(juce::Colours::white);
        g.setFont(9.0f);
        g.drawText(juce::String(block.stackGroup + 1), badge, juce::Justification::centred);
    }

    // Done indicator
    if (block.isDone) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
        g.setFont(10.0f);
        g.drawText("DONE", getLocalBounds().removeFromBottom(16), juce::Justification::centred);
    }
}

void BlockComponent::resized() {
    nameLabel.setBounds(getLocalBounds().reduced(4).withTrimmedTop(8).withTrimmedBottom(18));
}

void BlockComponent::setSelected(bool s) {
    if (selected == s) return;
    selected = s;
    repaint();
}

void BlockComponent::setHighlighted(bool h) {
    if (highlighted == h) return;
    highlighted = h;
    repaint();
}

void BlockComponent::setPlaying(bool p) {
    if (playing == p) return;
    playing = p;
    repaint();
}

void BlockComponent::mouseDown(const juce::MouseEvent& e) {
    // isPopupMenu() covers both physical right-click AND Control+click on macOS
    if (e.mods.isPopupMenu()) { showContextMenu(); return; }
    if (onSelected) onSelected(&block);
}

void BlockComponent::mouseDrag(const juce::MouseEvent& e) {
    if (e.getDistanceFromDragStart() > 8) {
        if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor(this)) {
            if (!dc->isDragAndDropActive()) {
                juce::Image img(juce::Image::ARGB, getWidth(), getHeight(), true);
                {
                    juce::Graphics ig(img);
                    ig.setColour(block.color.withAlpha(0.75f));
                    ig.fillRoundedRectangle(img.getBounds().toFloat().reduced(1.0f), 5.0f);
                    ig.setColour(juce::Colours::white);
                    ig.setFont(juce::Font(12.0f, juce::Font::bold));
                    ig.drawText(block.name, img.getBounds(), juce::Justification::centred);
                }
                dc->startDragging("block:" + block.id, this, juce::ScaledImage(img));
            }
        }
    }
}

void BlockComponent::mouseDoubleClick(const juce::MouseEvent&) {
    nameLabel.showEditor();
}

void BlockComponent::showContextMenu() {
    auto palette = LookAndFeel_BlockShuffler::getBlockPalette();
    juce::StringArray colourNames { "Red","Orange","Yellow","Green",
                                    "Cyan","Blue","Purple","Pink" };
    juce::PopupMenu colourMenu;
    for (int i = 0; i < palette.size() && i < colourNames.size(); ++i)
        colourMenu.addItem(20 + i, colourNames[i], true, block.color == palette[i]);

    juce::PopupMenu menu;
    menu.addItem(1, "Rename");
    menu.addSubMenu("Set Color", colourMenu);
    menu.addSeparator();
    menu.addItem(2, "Link to...");
    menu.addItem(7, "Remove Links");
    menu.addItem(3, "Stack with...");
    menu.addSeparator();
    menu.addItem(6, "Set as Overlapping", true, block.isOverlapping);
    menu.addItem(4, "Mark as Done",       true, block.isDone);
    menu.addSeparator();
    menu.addItem(5, "Delete Block");

    // Capture a pre-change snapshot now so the undo action restores the state
    // from before the user made their selection.
    juce::var pre = onCaptureSnapshot ? onCaptureSnapshot() : juce::var{};

    menu.showMenuAsync(juce::PopupMenu::Options{},
                       [this, palette, pre](int result) {
        if (result == 1) {
            nameLabel.showEditor();
        } else if (result >= 20 && result < 20 + palette.size()) {
            block.color = palette[result - 20];
            repaint();
            if (onUndoableMutation) onUndoableMutation(pre);
        } else if (result == 2) {
            if (onLinkRequested)  onLinkRequested(block.id);
        } else if (result == 3) {
            if (onStackRequested) onStackRequested(block.id);
        } else if (result == 4) {
            block.isDone = !block.isDone;
            repaint();
            if (onUndoableMutation) onUndoableMutation(pre);
        } else if (result == 6) {
            block.isOverlapping = !block.isOverlapping;
            repaint();
            if (onUndoableMutation) onUndoableMutation(pre);
        } else if (result == 7) {
            if (onRemoveLinksRequested) onRemoveLinksRequested(block.id);
        } else if (result == 5) {
            if (onDeleteRequested) onDeleteRequested(block.id);
        }
    });
}

} // namespace BlockShuffler
