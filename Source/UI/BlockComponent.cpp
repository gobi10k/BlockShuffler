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
    nameLabel.setFont(LookAndFeel_BlockShuffler::uiFontBold(12.0f));
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

    static constexpr float cr   = 7.0f;
    static constexpr int   hdrH = 20;
    static constexpr int   botH = 14;
    const auto full  = getLocalBounds();
    const auto inner = full.toFloat().reduced(1.0f);
    const bool active = (selected || highlighted);

    // ── 1. Base background ────────────────────────────────────────────────────
    auto bg = selected    ? juce::Colour(LookAndFeel_BlockShuffler::bgLight)
             : highlighted ? juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.22f)
             : hovered     ? juce::Colour(LookAndFeel_BlockShuffler::bgLight)
                           : juce::Colour(LookAndFeel_BlockShuffler::bgMedium);
    if (block->isDone) bg = bg.withAlpha(0.45f);
    g.setColour(bg);
    g.fillRoundedRectangle(inner, cr);

    // Subtle identity-color tint in body
    g.setColour(block->color.withAlpha(block->isDone ? 0.05f : 0.10f));
    g.fillRoundedRectangle(inner, cr);

    // ── 2. Header strip (rounded top corners only) ────────────────────────────
    auto headerF = juce::Rectangle<float>(inner.getX(), inner.getY(),
                                          inner.getWidth(), (float)hdrH);
    juce::Path hdrPath;
    hdrPath.addRoundedRectangle(headerF.getX(), headerF.getY(),
                                headerF.getWidth(), headerF.getHeight(),
                                cr, cr, true, true, false, false);
    g.setColour(block->color.withAlpha(block->isDone ? 0.5f : 1.0f));
    g.fillPath(hdrPath);

    // Block number — left of header
    g.setFont(LookAndFeel_BlockShuffler::monoFont(9.0f));
    g.setColour(juce::Colours::white.withAlpha(0.80f));
    g.drawText(juce::String(block->position + 1),
               juce::Rectangle<int>(full.getX() + 5, full.getY(), 18, hdrH),
               juce::Justification::centredLeft);

    // Clip count or stack-group badge — right of header
    if (block->stackGroup >= 0) {
        auto badge = juce::Rectangle<float>(inner.getRight() - 15.0f,
                                            inner.getY() + 4.0f, 12.0f, 12.0f);
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.fillEllipse(badge);
        g.setColour(juce::Colours::white);
        g.setFont(LookAndFeel_BlockShuffler::monoFont(8.0f));
        g.drawText(juce::String(block->stackGroup + 1),
                   badge.toNearestInt(), juce::Justification::centred);
    } else {
        g.setFont(LookAndFeel_BlockShuffler::monoFont(9.0f));
        g.setColour(juce::Colours::white.withAlpha(0.65f));
        g.drawText(juce::String(block->clips.size()),
                   juce::Rectangle<int>(full.getRight() - 22, full.getY(), 18, hdrH),
                   juce::Justification::centredRight);
    }

    // ── 3. Playing indicator (3px accent bar at very top) ────────────────────
    if (playing) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::startMarkerCol));
        g.fillRect(full.withHeight(3));
    }

    // ── 4. Border ─────────────────────────────────────────────────────────────
    if (active) {
        // 2px accent ring for selected / highlighted state
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol));
        if (block->isOverlapping) {
            juce::Path sp; sp.addRoundedRectangle(inner, cr);
            juce::Path dp; float dl[] = {6.0f, 3.0f};
            juce::PathStrokeType(2.0f).createDashedStroke(dp, sp, dl, 2);
            g.fillPath(dp);
        } else {
            g.drawRoundedRectangle(inner, cr, 2.0f);
        }
    } else {
        // Identity-color border — brightens on hover
        float ba = block->isDone ? 0.30f : (hovered ? 0.85f : 0.55f);
        g.setColour(block->color.withAlpha(ba));
        if (block->isOverlapping) {
            juce::Path sp; sp.addRoundedRectangle(inner, cr);
            juce::Path dp; float dl[] = {5.0f, 3.0f};
            juce::PathStrokeType(1.0f).createDashedStroke(dp, sp, dl, 2);
            g.fillPath(dp);
        } else {
            g.drawRoundedRectangle(inner, cr, 1.0f);
        }
    }

    // ── 5. Bottom readout ─────────────────────────────────────────────────────
    auto botArea = juce::Rectangle<int>(full.getX(), full.getBottom() - botH,
                                        full.getWidth(), botH);
    g.setFont(LookAndFeel_BlockShuffler::monoFont(9.0f));
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textTertiary)
                   .withAlpha(block->isDone ? 0.5f : 1.0f));
    if (block->tempo > 0.0)
        g.drawText(juce::String((int)block->tempo) + " bpm",
                   botArea.withTrimmedLeft(5).withTrimmedRight(22),
                   juce::Justification::centredLeft);
    if (block->isDone)
        g.drawText("done", botArea.withTrimmedRight(4), juce::Justification::centredRight);

    // ── 6. Clip-drop highlight ────────────────────────────────────────────────
    if (clipDropHighlight) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentAmber).withAlpha(0.9f));
        g.drawRoundedRectangle(full.toFloat().reduced(2.0f), cr, 2.5f);
        g.setFont(LookAndFeel_BlockShuffler::uiFontBold(10.0f));
        g.drawText("ADD CLIP", full.withTrimmedTop(full.getHeight() / 2),
                   juce::Justification::centred);
    }
}

void BlockComponent::resized() {
    nameLabel.setBounds(getLocalBounds().reduced(4).withTrimmedTop(20).withTrimmedBottom(16));
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

void BlockComponent::mouseEnter(const juce::MouseEvent&) {
    hovered = true;
    repaint();
}

void BlockComponent::mouseExit(const juce::MouseEvent&) {
    hovered = false;
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
