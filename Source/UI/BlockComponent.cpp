#include "BlockComponent.h"
#include "LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

// Issue-2 fix (stacked name obscured): on a non-tiny tile the below-header name
// area is compH - 44 px (4px border insets + 20px header + 16px bottom readout),
// so it shrinks as stack tiles shrink. Once it can't hold ONE legible 12pt-bold
// line plus label padding (~28px — about the header's own 20px text band plus
// insets), the name renders INSIDE the header band instead. Measured ground
// truth: nameH=24 (comp 68) already clips; nameH=43 (comp 87) is legible —
// 28 splits those with margin on both sides.
static constexpr int belowHeaderNameMinH = 28;

static inline bool nameGoesInHeader(int compH) noexcept {
    return compH > 26 && (compH - 44) < belowHeaderNameMinH;   // non-tiny && squeezed
}

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
    const bool active    = (selected || highlighted);
    const bool tinyTile  = (full.getHeight() <= 26);  // compact rendering for small stacks

    // ── 1. Base background ────────────────────────────────────────────────────
    auto bg = selected    ? juce::Colour(LookAndFeel_BlockShuffler::bgLight)
             : highlighted ? juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.22f)
             : hovered     ? juce::Colour(LookAndFeel_BlockShuffler::bgLight)
                           : juce::Colour(LookAndFeel_BlockShuffler::bgMedium);
    g.setColour(bg);
    g.fillRoundedRectangle(inner, cr);

    // Body identity colour for full-size tiles: the palette hue rendered
    // DOMINANT (0.85) over a NEUTRAL grey base. Compositing only 10% of the
    // colour over the cool blue-grey theme base desaturated and hue-shifted
    // every colour (bug 13.1: yellow read as green). Text colour follows the
    // body luminance (same rule as the clip-row headers) so the name stays
    // legible on both light and dark colours.
    static constexpr juce::uint32 neutralBodyBase = 0xFF3A3A3A;  // neutral grey, no hue
    const juce::Colour bodyCol =
        juce::Colour(neutralBodyBase).interpolatedWith(block->color, 0.85f);
    const float bodyLum = bodyCol.getFloatRed()   * 0.299f
                        + bodyCol.getFloatGreen() * 0.587f
                        + bodyCol.getFloatBlue()  * 0.114f;
    const juce::Colour bodyTextCol = (bodyLum > 0.55f) ? juce::Colours::black
                                                       : juce::Colours::white;
    // Tiny (stacked) tiles keep the existing textPrimary name colour; only the
    // large-body path adopts the luminance-based colour.
    juce::Colour nameCol = tinyTile
        ? juce::Colour(LookAndFeel_BlockShuffler::textPrimary)
        : bodyTextCol;
    if (!tinyTile && nameGoesInHeader(full.getHeight())) {
        // Issue-2 fix: in header mode the name sits on the SOLID block colour of
        // the header band — follow the header's luminance, not the body's.
        const float hdrLum = block->color.getFloatRed()   * 0.299f
                           + block->color.getFloatGreen() * 0.587f
                           + block->color.getFloatBlue()  * 0.114f;
        nameCol = (hdrLum > 0.55f) ? juce::Colours::black : juce::Colours::white;
    }
    if (nameLabel.findColour(juce::Label::textColourId) != nameCol)
        nameLabel.setColour(juce::Label::textColourId, nameCol);

    if (tinyTile) {
        // Compact mode: full-height color strip + name only
        g.setColour(block->color.withAlpha(0.80f));
        g.fillRoundedRectangle(inner, juce::jmin(cr, (float)full.getHeight() * 0.4f));
    } else {
        // Identity colour DOMINANT over the neutral base (see bug 13.1 above)
        g.setColour(bodyCol);
        g.fillRoundedRectangle(inner, cr);

        // ── 2. Header strip (rounded top corners only) ────────────────────────
        auto headerF = juce::Rectangle<float>(inner.getX(), inner.getY(),
                                              inner.getWidth(), (float)hdrH);
        juce::Path hdrPath;
        hdrPath.addRoundedRectangle(headerF.getX(), headerF.getY(),
                                    headerF.getWidth(), headerF.getHeight(),
                                    cr, cr, true, true, false, false);
        g.setColour(block->color.withAlpha(1.0f));
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
    }

    // ── 3. Playing indicator — white border + dark inner line, readable on any colour ──
    if (playing) {
        g.setColour(juce::Colours::white);
        g.drawRect(getLocalBounds(), 3);
        g.setColour(juce::Colour(0x44000000));
        g.drawRect(getLocalBounds().reduced(3), 1);
    }

    // ── 4. Border ─────────────────────────────────────────────────────────────
    const float borderCr = tinyTile ? juce::jmin(cr, (float)full.getHeight() * 0.4f) : cr;
    if (active) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol));
        g.drawRoundedRectangle(inner, borderCr, 2.0f);
    } else {
        float ba = hovered ? 0.85f : 0.55f;
        g.setColour(block->color.withAlpha(tinyTile ? 0.0f : ba));  // border already merged into bg for tiny
        if (!tinyTile)
            g.drawRoundedRectangle(inner, borderCr, 1.0f);
    }

    // ── 5. Bottom readout (full-size tiles only) ──────────────────────────────
    if (!tinyTile) {
        auto botArea = juce::Rectangle<int>(full.getX(), full.getBottom() - botH,
                                            full.getWidth(), botH);
        g.setFont(LookAndFeel_BlockShuffler::monoFont(9.0f));
        g.setColour(bodyTextCol.withAlpha(0.70f));  // legible on the vivid body (13.1)
        if (block->tempo > 0.0)
            g.drawText(juce::String((int)block->tempo) + " bpm",
                       botArea.withTrimmedLeft(5).withTrimmedRight(22),
                       juce::Justification::centredLeft);
    }

    // ── 6. Clip-drop highlight ────────────────────────────────────────────────
    if (clipDropHighlight) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentAmber).withAlpha(0.9f));
        g.drawRoundedRectangle(full.toFloat().reduced(2.0f), cr, 2.5f);
        g.setFont(LookAndFeel_BlockShuffler::uiFontBold(10.0f));
        g.drawText("ADD CLIP", full.withTrimmedTop(full.getHeight() / 2),
                   juce::Justification::centred);
    }

    // ── 7. Done overlay — dim + diagonal line + corner badge, name stays readable ──
    if (block->isDone) {
        g.setColour(juce::Colour(0x66000000));
        g.fillRect(getLocalBounds());

        g.setColour(juce::Colour(0x88FF4444));
        g.drawLine(0.0f, 0.0f, (float)getWidth(), (float)getHeight(), 1.5f);

        if (!tinyTile) {
            auto badgeBounds = getLocalBounds().removeFromBottom(16).removeFromRight(40);
            g.setColour(juce::Colour(0xCC444444));
            g.fillRoundedRectangle(badgeBounds.toFloat(), 3.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText("DONE", badgeBounds, juce::Justification::centred);
        }
    }
}

void BlockComponent::resized() {
    if (getHeight() <= 26) {
        nameLabel.setFont(LookAndFeel_BlockShuffler::uiFontBold(9.0f));
        nameLabel.setMinimumHorizontalScale(0.0f);   // JUCE default (squish to 0.7)
        nameLabel.setBounds(getLocalBounds().reduced(2));
    } else if (nameGoesInHeader(getHeight())) {
        // Issue-2 fix: below-header area too short for a legible line — put the
        // name INSIDE the 20px header band, between the block-number indicator
        // (left, ends x=23) and the stack badge / clip count (right, starts
        // x = w-22). Scale 1.0 = truncate with ellipsis, never squish.
        nameLabel.setFont(LookAndFeel_BlockShuffler::uiFontBold(11.0f));
        nameLabel.setMinimumHorizontalScale(1.0f);
        nameLabel.setBounds(juce::Rectangle<int>(24, 1, juce::jmax(0, getWidth() - 48), 20));
    } else {
        nameLabel.setFont(LookAndFeel_BlockShuffler::uiFontBold(12.0f));
        nameLabel.setMinimumHorizontalScale(0.0f);   // JUCE default (squish to 0.7)
        nameLabel.setBounds(getLocalBounds().reduced(4).withTrimmedTop(20).withTrimmedBottom(16));
    }
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

    // Capture everything the drop handler needs into locals, and reset ALL member
    // state, BEFORE calling onDragEnded(). The strip's rebuild is deferred so this
    // component is no longer freed during onDragEnded — but as defence in depth we
    // touch NO member (and read nothing from `this`) after the call, so a future
    // change can never reintroduce the 12.1 use-after-free here.
    const bool shiftDrag  = isShiftDrag;
    // Capture the drop centre BEFORE snapping back; getBounds() changes on snap.
    const auto dropCentre = getBounds().getCentre();

    isDragging  = false;
    isShiftDrag = false;

    // Snap the tile back to where it started; BlockStrip's rebuild will re-layout.
    setTopLeftPosition(dragStartPos);

    if (onDragEnded)
        onDragEnded(this, dropCentre, shiftDrag);
    // NOTE: do not access any member of this component below this line.
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

juce::String BlockComponent::getTooltip() {
    if (block && block->stackGroup >= 0 && !isDragging)
        return "Hold Shift and drag to move the entire stack";
    return juce::SettableTooltipClient::getTooltip();
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
