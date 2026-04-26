#include "BlockComponent.h"
#include "LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

BlockComponent::BlockComponent(Block& block_,
                               std::function<void(Block*)>              onSelected_,
                               std::function<void(const juce::String&)> onDeleteRequested_,
                               std::function<void()>                    onMutated_,
                               std::function<void(const juce::String&)> onLinkRequested_,
                               std::function<void(const juce::String&)> onStackRequested_)
    : block(&block_),
      onSelected(std::move(onSelected_)),
      onDeleteRequested(std::move(onDeleteRequested_)),
      onMutated(std::move(onMutated_)),
      onLinkRequested(std::move(onLinkRequested_)),
      onStackRequested(std::move(onStackRequested_))
{
    nameLabel.setText(block ? block->name : "", juce::dontSendNotification);
    nameLabel.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
    nameLabel.setColour(juce::Label::textColourId,
                        juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
    nameLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setEditable(false, true, false);
    nameLabel.onEditorShow = [this] {
        if (block) {
            nameBeforeEdit = block->name;
            namePre = onCaptureSnapshot ? onCaptureSnapshot() : juce::var{};
        }
    };
    nameLabel.onTextChange = [this] {
        if (block) block->name = nameLabel.getText();
        if (onMutated) onMutated();
    };
    nameLabel.onEditorHide = [this] {
        // In JUCE 8, onEditorHide fires before onTextChange, so block.name still
        // holds the old value here. Read the committed text directly from the label.
        juce::String newName = nameLabel.getText();
        if (newName != nameBeforeEdit && !namePre.isVoid() && block) {
            block->name = newName;  // update now so toJSON() inside recordMutation sees the new name
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
    if (!block) return;
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);

    // Base background
    auto bg = selected    ? juce::Colour(LookAndFeel_BlockShuffler::bgLight)
              : highlighted ? juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.25f)
                            : juce::Colour(LookAndFeel_BlockShuffler::bgMedium);
    if (block->isDone) bg = bg.withAlpha(0.45f);
    g.setColour(bg);
    g.fillRoundedRectangle(bounds, 5.0f);

    // Subtle block-color tint over the background
    if (!block->isDone)
        g.setColour(block->color.withAlpha(0.12f));
    else
        g.setColour(block->color.withAlpha(0.06f));
    g.fillRoundedRectangle(bounds, 5.0f);

    // Colored top bar — always the block's own color (playing indicator is drawn separately below)
    g.setColour(block->color);
    g.fillRoundedRectangle(bounds.removeFromTop(8.0f), 3.0f);

    // PLAYING indicator — bright green bar at the very top (informational, does not affect selection)
    // This tells the user which block is currently sounding; it is independent of the selection state.
    if (playing) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::startMarkerCol));
        g.fillRect(getLocalBounds().removeFromTop(4));
    }

    // EDITING / SELECTED indicator — blue border around the entire block tile.
    // This is set only by user clicks and controls which block the inspector shows.
    // Drawn on getLocalBounds() (full tile) so it is always visible regardless of playing state.
    if (highlighted || selected) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol));
        float borderW = 2.0f;
        if (block->isOverlapping) {
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
    } else {
        // Subtle block-color border when not selected
        g.setColour(block->color.withAlpha(0.6f));
        if (block->isOverlapping) {
            juce::Path solidPath;
            solidPath.addRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 5.0f);
            juce::Path dashedPath;
            float dashLengths[] = { 5.0f, 3.0f };
            juce::PathStrokeType(1.0f).createDashedStroke(dashedPath, solidPath,
                                                            dashLengths, 2);
            g.fillPath(dashedPath);
        } else {
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 5.0f, 1.0f);
        }
    }

    // Stack badge (top-right)
    if (block->stackGroup >= 0) {
        auto badge = getLocalBounds().removeFromTop(16).removeFromRight(16);
        g.setColour(block->color);
        g.fillEllipse(badge.toFloat().reduced(1.0f));
        g.setColour(juce::Colours::white);
        g.setFont(9.0f);
        g.drawText(juce::String(block->stackGroup + 1), badge, juce::Justification::centred);
    }

    // Done indicator
    if (block->isDone) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textSecondary));
        g.setFont(10.0f);
        g.drawText("DONE", getLocalBounds().removeFromBottom(16), juce::Justification::centred);
    }

    // Clip-drop highlight: amber border + "ADD CLIP" label
    if (clipDropHighlight) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentAmber).withAlpha(0.9f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 5.0f, 2.5f);
        g.setFont(10.0f);
        g.drawText("ADD CLIP", getLocalBounds().withTrimmedTop(getHeight() / 2),
                   juce::Justification::centred);
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

// ── Mouse handling — ComponentDragger-based block drag ────────────────────────

void BlockComponent::mouseDown(const juce::MouseEvent& e) {
    // isPopupMenu() covers both physical right-click AND Control+click on macOS.
    if (e.mods.isPopupMenu()) { showContextMenu(); return; }

    // Store starting position for snap-back after the drag ends.
    // Selection is deferred to mouseUp so a drag doesn't simultaneously select.
    isDragging   = false;
    dragStartPos = getPosition();
    dragger.startDraggingComponent(this, e);
}

void BlockComponent::mouseDrag(const juce::MouseEvent& e) {
    if (e.getDistanceFromDragStart() < 5) return;

    if (!isDragging) {
        isDragging  = true;
        isShiftDrag = e.mods.isShiftDown();

        // Record sibling start positions first so the dragged tile ends up on top.
        if (isShiftDrag && block && block->stackGroup >= 0 && onBeginStackDrag)
            onBeginStackDrag(block->stackGroup);

        toFront(true);  // dragged tile on top of everything, including siblings
    }

    dragger.dragComponent(this, e, nullptr);

    // Reposition sibling stack tiles by the same pixel delta.
    if (isShiftDrag && block && block->stackGroup >= 0 && onMoveStackComponents)
        onMoveStackComponents(block->stackGroup, this, getPosition() - dragStartPos);

    // Report current centre to the parent strip for visual feedback.
    if (onDragMoved)
        onDragMoved(this, getBounds().getCentre(), isShiftDrag);
}

void BlockComponent::mouseUp(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);

    if (!isDragging) {
        // Short click without threshold drag — treat as a selection.
        if (onSelected) onSelected(block.get());
        return;
    }

    isDragging = false;

    // Capture the drop centre BEFORE snapping back; getBounds() changes on snap.
    auto dropCentre = getBounds().getCentre();

    // Snap the tile back to where it started; BlockStrip's rebuild will re-layout.
    setTopLeftPosition(dragStartPos);

    if (onDragEnded)
        onDragEnded(this, dropCentre, isShiftDrag);

    isShiftDrag = false;
}

void BlockComponent::mouseDoubleClick(const juce::MouseEvent&) {
    nameLabel.showEditor();
}

// ── DragAndDropTarget — clip drops only ───────────────────────────────────────

bool BlockComponent::isInterestedInDragSource(const SourceDetails& details) {
    return details.description.toString().startsWith("clip:");
}

void BlockComponent::itemDragEnter(const SourceDetails& details) {
    if (!details.description.toString().startsWith("clip:")) return;
    clipDropHighlight = true;
    repaint();
}

void BlockComponent::itemDragMove(const SourceDetails&) {
    // Nothing to update for clip drags
}

void BlockComponent::itemDragExit(const SourceDetails&) {
    clipDropHighlight = false;
    repaint();
}

void BlockComponent::itemDropped(const SourceDetails& details) {
    clipDropHighlight = false;
    repaint();
    if (!block) return;
    auto desc = details.description.toString();
    if (desc.startsWith("clip:")) {
        auto clipId = desc.substring(5);
        if (onClipDropped) onClipDropped(clipId, block->id);
    }
}

// ── Context menu ──────────────────────────────────────────────────────────────

void BlockComponent::showContextMenu() {
    auto palette = LookAndFeel_BlockShuffler::getBlockPalette();
    juce::StringArray colourNames { "Red","Orange","Yellow","Green",
                                    "Cyan","Blue","Purple","Pink" };
    juce::PopupMenu colourMenu;
    for (int i = 0; i < palette.size() && i < colourNames.size(); ++i)
        colourMenu.addItem(20 + i, colourNames[i], true, block && block->color == palette[i]);

    juce::PopupMenu menu;
    menu.addItem(1, "Rename");
    menu.addSubMenu("Set Color", colourMenu);
    menu.addSeparator();
    menu.addItem(8, "Play from Here");
    menu.addItem(10, "Play Block");
    menu.addSeparator();
    menu.addItem(2, "Link to...");
    menu.addItem(7, "Remove Links");
    menu.addItem(3, "Stack with...");
    menu.addItem(9, "Unstack", block && block->stackGroup >= 0);
    menu.addSeparator();
    menu.addItem(6, "Set as Overlapping", true, block && block->isOverlapping);
    menu.addItem(4, "Mark as Done",       true, block && block->isDone);
    menu.addSeparator();
    menu.addItem(5, "Delete Block");

    juce::var pre = onCaptureSnapshot ? onCaptureSnapshot() : juce::var{};

    juce::Component::SafePointer<BlockComponent> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options{},
                       [safeThis, palette, pre](int result) {
        if (!safeThis) return;
        auto* self = safeThis.getComponent();
        if (result == 1) {
            self->nameLabel.showEditor();
        } else if (result >= 20 && result < 20 + palette.size() && self->block) {
            self->block->color = palette[result - 20];
            self->repaint();
            if (self->onUndoableMutation) self->onUndoableMutation(pre);
        } else if (result == 2 && self->block) {
            if (self->onLinkRequested)  self->onLinkRequested(self->block->id);
        } else if (result == 3 && self->block) {
            if (self->onStackRequested) self->onStackRequested(self->block->id);
        } else if (result == 4 && self->block) {
            self->block->isDone = !self->block->isDone;
            self->repaint();
            if (self->onUndoableMutation) self->onUndoableMutation(pre);
        } else if (result == 6 && self->block) {
            self->block->isOverlapping = !self->block->isOverlapping;
            self->repaint();
            if (self->onUndoableMutation) self->onUndoableMutation(pre);
        } else if (result == 7 && self->block) {
            if (self->onRemoveLinksRequested) self->onRemoveLinksRequested(self->block->id);
        } else if (result == 8 && self->block) {
            if (self->onPlayFromHereRequested) self->onPlayFromHereRequested(self->block->id);
        } else if (result == 10 && self->block) {
            if (self->onPlayBlockRequested) self->onPlayBlockRequested(self->block->id);
        } else if (result == 9 && self->block && self->block->stackGroup >= 0) {
            self->block->stackGroup = -1;
            if (self->onMutated) self->onMutated();
            if (self->onUndoableMutation) self->onUndoableMutation(pre);
        } else if (result == 5 && self->block) {
            if (self->onDeleteRequested) self->onDeleteRequested(self->block->id);
        }
    });
}

} // namespace BlockShuffler
