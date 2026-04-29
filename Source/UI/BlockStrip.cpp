#include "BlockStrip.h"
#include "LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

BlockStrip::~BlockStrip() {
    if (project) project->removeChangeListener(this);
}

void BlockStrip::init(Project& proj, BlockLinkOverlay* ov) {
    project = &proj;
    overlay = ov;
    project->addChangeListener(this);
    setOpaque(true);
    setWantsKeyboardFocus(true);

    viewport.setViewedComponent(&contentArea, false);
    viewport.setScrollBarsShown(false, true);
    viewport.setScrollBarThickness(8);
    viewport.onScrollChanged = [this] { updateOverlay(); };
    addAndMakeVisible(viewport);

    addButton.onClick = [this] {
        if (!project) return;
        cancelPendingMode();
        auto* newBlock = project->addBlock();
        // Rebuild synchronously so the new block has a component with valid bounds
        // before selectBlock() runs and before the user can start dragging.
        rebuildBlocks();
        resized();
        selectBlock(newBlock);
    };
    addButton.setTooltip("Add a new block");
    addAndMakeVisible(addButton);

    // Mode label renders on top of the viewport
    modeLabel.setJustificationType(juce::Justification::centred);
    modeLabel.setFont(LookAndFeel_BlockShuffler::uiFont(12.0f));
    modeLabel.setColour(juce::Label::textColourId,
                        juce::Colour(LookAndFeel_BlockShuffler::accentCol));
    modeLabel.setColour(juce::Label::backgroundColourId,
                        juce::Colour(LookAndFeel_BlockShuffler::bgDark).withAlpha(0.85f));
    modeLabel.setInterceptsMouseClicks(false, false);
    modeLabel.setVisible(false);
    addAndMakeVisible(modeLabel);

    rebuildBlocks();
}

// ── Painting ──────────────────────────────────────────────────────────────────

void BlockStrip::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(LookAndFeel_BlockShuffler::bgMedium));

    // Linking/stacking pending-mode tint
    if (pendingMode != PendingMode::None) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.15f));
        g.fillRect(getLocalBounds());
    }
}

void BlockStrip::paintOverChildren(juce::Graphics& g) {
    // Helpers: convert contentArea-local coords to BlockStrip-local coords.
    auto contentToStripX = [this](int cx) {
        return cx + viewport.getX() - viewport.getViewPositionX();
    };
    auto contentToStripY = [this](int cy) {
        return cy + viewport.getY();
    };

    if (currentDropAction == DropAction::Stack
        && dropTargetComp != nullptr
        && dropTargetIndex >= 0
        && dropTargetIndex < originalBounds.size()) {
        // Teal highlight: "drop here to stack with a different block"
        auto cb = originalBounds[dropTargetIndex];
        auto r  = juce::Rectangle<int>(contentToStripX(cb.getX()), contentToStripY(cb.getY()),
                                       cb.getWidth(), cb.getHeight());
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentTeal).withAlpha(0.85f));
        g.drawRoundedRectangle(r.toFloat().reduced(2.0f), 7.0f, 2.5f);
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
        g.setFont(LookAndFeel_BlockShuffler::uiFontBold(10.0f));
        g.drawText("STACK", r.withTrimmedTop(r.getHeight() / 2), juce::Justification::centred);

    } else if (currentDropAction == DropAction::RearrangeInStack
               && dropTargetComp != nullptr
               && dropTargetIndex >= 0
               && dropTargetIndex < originalBounds.size()) {
        // Green highlight: "drop here to swap positions within this stack"
        auto cb = originalBounds[dropTargetIndex];
        auto r  = juce::Rectangle<int>(contentToStripX(cb.getX()), contentToStripY(cb.getY()),
                                       cb.getWidth(), cb.getHeight());
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::startMarkerCol).withAlpha(0.3f));
        g.fillRoundedRectangle(r.toFloat().reduced(2.0f), 7.0f);
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::startMarkerCol).withAlpha(0.9f));
        g.drawRoundedRectangle(r.toFloat().reduced(2.0f), 7.0f, 2.5f);
        g.setColour(juce::Colours::white);
        g.setFont(LookAndFeel_BlockShuffler::uiFontBold(10.0f));
        g.drawText("SWAP", r.withTrimmedTop(r.getHeight() / 2), juce::Justification::centred);

    } else if (currentDropAction == DropAction::Reorder && !originalBounds.isEmpty()) {
        // Vertical insertion line: teal for single-block reorder, amber+thicker for stack move.
        int insertBefore = dropTargetIndex;
        int lineContentX;
        if (insertBefore <= 0) {
            lineContentX = originalBounds[0].getX() - blockGap / 2;
        } else if (insertBefore >= originalBounds.size()) {
            lineContentX = originalBounds.getLast().getRight() + blockGap / 2;
        } else {
            auto prev = originalBounds[insertBefore - 1];
            auto next = originalBounds[insertBefore];
            lineContentX = (prev.getRight() + next.getX()) / 2;
        }
        int lineX = contentToStripX(lineContentX);
        auto lineColour = isStackMove
            ? juce::Colour(LookAndFeel_BlockShuffler::accentAmber).withAlpha(0.9f)
            : juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.9f);
        float lineW = isStackMove ? 4.0f : 2.5f;
        g.setColour(lineColour);
        g.drawLine(float(lineX), float(viewport.getY() + 4),
                   float(lineX), float(viewport.getBottom() - 4), lineW);
        g.fillEllipse(float(lineX - 4), float(viewport.getY() + 2),       8.0f, 8.0f);
        g.fillEllipse(float(lineX - 4), float(viewport.getBottom() - 10), 8.0f, 8.0f);
    }
}

// ── Layout ────────────────────────────────────────────────────────────────────

void BlockStrip::resized() {
    if (!project) return;

    // blockComponents must mirror project->blocks before we index into it.
    if (blockComponents.size() != project->blocks.size())
        rebuildBlocks();

    auto area = getLocalBounds().reduced(padding, 0);

    addButton.setBounds(area.removeFromRight(addBtnW)
                            .withSizeKeepingCentre(addBtnW, 28));
    area.removeFromRight(padding);
    viewport.setBounds(area);

    // Mode label spans the full viewport width, pinned at the top of the strip
    modeLabel.setBounds(area.withHeight(20));

    const int areaH = area.getHeight();

    // Build slot groups: blocks with the same stackGroup share a horizontal slot.
    struct Slot { juce::Array<int> indices; };
    juce::Array<Slot> slots;
    juce::HashMap<int, int> stackGroupToSlot;

    for (int i = 0; i < project->blocks.size(); ++i) {
        int sg = project->blocks[i]->stackGroup;
        if (sg < 0) {
            Slot s; s.indices.add(i); slots.add(std::move(s));
        } else {
            if (stackGroupToSlot.contains(sg)) {
                slots.getReference(stackGroupToSlot[sg]).indices.add(i);
            } else {
                stackGroupToSlot.set(sg, slots.size());
                Slot s; s.indices.add(i); slots.add(std::move(s));
            }
        }
    }

    int numSlots = slots.size();
    int totalW   = juce::jmax(numSlots * (blockW + blockGap), area.getWidth());
    contentArea.setBounds(0, 0, totalW, areaH);

    blockCentreXCache.resize(project->blocks.size());
    originalBounds.resize(project->blocks.size());

    int x = 0;
    for (auto& slot : slots) {
        int n = slot.indices.size();
        // Each block in the stack gets an equal share of the slot height,
        // with a 4-px gap between stacked tiles so they're visually distinct.
        int totalGaps = (n - 1) * 4;
        int perH      = (areaH - totalGaps) / n;
        int startY    = (areaH - (perH * n + totalGaps)) / 2;

        for (int j = 0; j < n; ++j) {
            int bi = slot.indices[j];
            int y  = startY + j * (perH + 4);
            auto bounds = juce::Rectangle<int>(x, y, blockW, perH);
            if (bi >= 0 && bi < blockComponents.size())
                blockComponents[bi]->setBounds(bounds);
            if (bi >= 0 && bi < blockCentreXCache.size())
                blockCentreXCache.set(bi, x + blockW / 2);
            if (bi >= 0 && bi < originalBounds.size())
                originalBounds.set(bi, bounds);
        }
        x += blockW + blockGap;
    }

    updateOverlay();
}

// ── Change listener ───────────────────────────────────────────────────────────

void BlockStrip::changeListenerCallback(juce::ChangeBroadcaster*) {
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<BlockStrip>(this)] {
        if (!safe) return;
        // If a block drag is active, the dragged BlockComponent lives inside
        // blockComponents. Rebuilding now would free it while its event handler
        // is still running → crash. Defer until the drag completes.
        if (safe->activeDragComp != nullptr) {
            safe->needsRebuildAfterDrag = true;
            return;
        }
        // Preserve the horizontal scroll position across rebuilds so that
        // dropping a clip onto a far-right block doesn't snap the view back to x=0.
        int savedScrollX = safe->viewport.getViewPositionX();
        safe->needsRebuildAfterDrag = false;
        safe->rebuildBlocks();
        safe->resized();
        safe->repaint();
        if (savedScrollX > 0)
            safe->viewport.setViewPosition(savedScrollX, 0);
    });
}

// ── Block component management ────────────────────────────────────────────────

void BlockStrip::rebuildBlocks() {
    if (!project) return;
    // Reset drag state — any component pointers are about to be freed.
    activeDragComp  = nullptr;
    dropTargetComp  = nullptr;
    currentDropAction = DropAction::None;
    dropTargetIndex   = -1;

    contentArea.removeAllChildren();
    blockComponents.clear();

    for (auto* block : project->blocks) {
        auto* bc = blockComponents.add(new BlockComponent(
            *block,
            [this](Block* b) {
                if (pendingMode != PendingMode::None) {
                    completePendingMode(b->id);
                } else {
                    selectBlock(b);
                }
            },
            [this](const juce::String& id) { deleteBlock(id); },
            [this] { repaint(); },
            [this](const juce::String& id) { enterLinkMode(id); },
            [this](const juce::String& id) { enterStackMode(id); }
        ));

        bc->onCaptureSnapshot  = [this] { return project ? project->toJSON() : juce::var{}; };
        bc->onUndoableMutation = [this](const juce::var& pre) {
            if (project) project->applyExternalMutation(pre);
        };
        bc->onRemoveLinksRequested = [this](const juce::String& id) {
            if (project) project->removeLinksForBlock(id);
        };
        bc->onPlayFromHereRequested = [this](const juce::String& id) {
            if (onPlayFromHereRequested) onPlayFromHereRequested(id);
        };
        bc->onPlayBlockRequested = [this](const juce::String& id) {
            if (onPlayBlockRequested) onPlayBlockRequested(id);
        };
        bc->onClipDropped = [this](const juce::String& clipId, const juce::String& targetBlockId) {
            if (onClipDropped) onClipDropped(clipId, targetBlockId);
        };

        // ── ComponentDragger callbacks ────────────────────────────────────────
        bc->onDragMoved = [this](BlockComponent* comp, juce::Point<int> centre, bool shiftDrag) {
            updateDragFeedback(comp, centre, shiftDrag);
        };
        bc->onDragEnded = [this](BlockComponent* comp, juce::Point<int> centre, bool shiftDrag) {
            blockDropped(comp, centre, shiftDrag);
        };
        bc->onBeginStackDrag = [this](int sg) {
            beginStackDrag(sg);
        };
        bc->onMoveStackComponents = [this](int sg, BlockComponent* dragged, juce::Point<int> delta) {
            moveStackComponents(sg, dragged, delta);
        };

        bc->setSelected(block->id == selectedBlockId);
        bc->setPlaying(block->id == playingBlockId);
        bool inPendingMode = (pendingMode != PendingMode::None);
        bool isSource      = (block->id == pendingBlockId);
        bc->setHighlighted(inPendingMode && !isSource);
        contentArea.addAndMakeVisible(bc);
    }
}

void BlockStrip::selectBlock(Block* block) {
    selectedBlockId = block ? block->id : juce::String{};
    for (auto* bc : blockComponents) {
        auto* bPtr = bc->getBlock();
        bc->setSelected(bPtr && bPtr->id == selectedBlockId);
    }

    // Scroll so the selected block is visible
    if (block) {
        int idx = 0;
        for (auto* b : project->blocks) { if (b->id == block->id) break; ++idx; }
        if (idx < blockCentreXCache.size()) {
            int cx = blockCentreXCache[idx];
            int vx = viewport.getViewPositionX();
            int vw = viewport.getMaximumVisibleWidth();
            if (cx - blockW / 2 < vx)
                viewport.setViewPosition(juce::jmax(0, cx - blockW / 2 - padding), 0);
            else if (cx + blockW / 2 > vx + vw)
                viewport.setViewPosition(cx + blockW / 2 + padding - vw, 0);
        }
    }

    if (onBlockSelected) onBlockSelected(block);
}

void BlockStrip::deleteBlock(const juce::String& blockId) {
    if (!project) return;
    cancelPendingMode();
    if (selectedBlockId == blockId) {
        selectedBlockId = {};
        if (onBlockSelected) onBlockSelected(nullptr);
    }
    project->removeBlock(blockId);
}

bool BlockStrip::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress(juce::KeyPress::escapeKey) &&
        pendingMode != PendingMode::None) {
        cancelPendingMode();
        return true;
    }
    return false;
}

// ── Link / stack pending modes ────────────────────────────────────────────────

void BlockStrip::enterLinkMode(const juce::String& fromBlockId) {
    pendingMode    = PendingMode::Link;
    pendingBlockId = fromBlockId;
    for (auto* bc : blockComponents) {
        auto* bPtr = bc->getBlock();
        bc->setHighlighted(bPtr && bPtr->id != fromBlockId);
    }
    if (overlay) {
        int idx = 0;
        for (auto* b : project->blocks) { if (b->id == fromBlockId) break; ++idx; }
        overlay->setLinkingSourceX(blockCentreX(idx));
    }
    modeLabel.setText("Click a block to link to it  (Esc to cancel)",
                      juce::dontSendNotification);
    modeLabel.setVisible(true);
    grabKeyboardFocus();
    repaint();
}

void BlockStrip::enterStackMode(const juce::String& fromBlockId) {
    pendingMode    = PendingMode::Stack;
    pendingBlockId = fromBlockId;
    for (auto* bc : blockComponents) {
        auto* bPtr = bc->getBlock();
        bc->setHighlighted(bPtr && bPtr->id != fromBlockId);
    }
    modeLabel.setText("Click a block to stack with it  (Esc to cancel)",
                      juce::dontSendNotification);
    modeLabel.setVisible(true);
    grabKeyboardFocus();
    repaint();
}

void BlockStrip::cancelPendingMode() {
    pendingMode    = PendingMode::None;
    pendingBlockId = {};
    for (auto* bc : blockComponents)
        bc->setHighlighted(false);
    if (overlay) overlay->setLinkingSourceX(-1);
    modeLabel.setVisible(false);
    repaint();
}

void BlockStrip::completePendingMode(const juce::String& targetBlockId) {
    if (targetBlockId == pendingBlockId) { cancelPendingMode(); return; }
    if (pendingMode == PendingMode::Link) {
        project->addLink(pendingBlockId, targetBlockId, 0.5f);
    } else if (pendingMode == PendingMode::Stack) {
        project->stackBlocks(pendingBlockId, targetBlockId);
    }
    cancelPendingMode();
}

// ── Overlay / utilities ───────────────────────────────────────────────────────

void BlockStrip::updateOverlay() {
    if (!overlay || !project) return;
    juce::HashMap<juce::String, int> positions;
    for (int i = 0; i < project->blocks.size(); ++i) {
        if (i >= blockCentreXCache.size()) break;
        int cx = viewport.getX() + blockCentreXCache[i]
                 - viewport.getViewPositionX() + padding;
        positions.set(project->blocks[i]->id, cx);
    }
    overlay->setProject(project);
    overlay->setBlockPositions(positions);
}

void BlockStrip::setPlayingBlock(const juce::String& blockId) {
    if (playingBlockId == blockId) return;
    playingBlockId = blockId;
    for (auto* bc : blockComponents) {
        auto* bPtr = bc->getBlock();
        bc->setPlaying(bPtr && bPtr->id == playingBlockId);
    }
}

Block* BlockStrip::getBlockAtLocalPoint(juce::Point<int> localPt) const {
    if (!project) return nullptr;
    auto contentPos = toContentPos(localPt);
    for (int i = 0; i < blockComponents.size(); ++i) {
        if (blockComponents[i]->getBounds().contains(contentPos))
            return (i < project->blocks.size()) ? project->blocks[i] : nullptr;
    }
    return nullptr;
}

int BlockStrip::blockCentreX(int blockIndex) const {
    if (blockIndex < 0 || blockIndex >= blockCentreXCache.size()) return 0;
    return viewport.getX() + blockCentreXCache[blockIndex]
           - viewport.getViewPositionX() + padding;
}

juce::Point<int> BlockStrip::toContentPos(juce::Point<int> stripLocal) const {
    return { stripLocal.x - viewport.getX() + viewport.getViewPositionX(),
             stripLocal.y - viewport.getY() };
}

// ── Block drag handling ───────────────────────────────────────────────────────

void BlockStrip::updateDragFeedback(BlockComponent* draggedComp, juce::Point<int> centre, bool shiftDrag) {
    if (!project) return;

    activeDragComp    = draggedComp;
    currentDropAction = DropAction::None;
    dropTargetIndex   = -1;
    dropTargetComp    = nullptr;
    isStackMove       = false;

    auto* draggedBlock = draggedComp->getBlock();
    if (draggedBlock == nullptr) { repaint(); return; }

    // Shift+drag on a stacked block: show a stack-move insertion line, skipping same-stack slots.
    if (shiftDrag && draggedBlock->stackGroup >= 0) {
        currentDropAction = DropAction::Reorder;
        isStackMove       = true;
        dropTargetIndex   = blockComponents.size();  // default: append
        for (int i = 0; i < blockComponents.size(); ++i) {
            if (i >= originalBounds.size()) continue;
            auto* b = blockComponents[i]->getBlock();
            if (b == nullptr || b->stackGroup == draggedBlock->stackGroup) continue;
            if (originalBounds[i].getCentreX() > centre.x) {
                dropTargetIndex = i;
                break;
            }
        }
        repaint();
        return;
    }

    // Check every other block's ORIGINAL (pre-drag) bounds for a hit.
    for (int i = 0; i < blockComponents.size(); ++i) {
        auto* comp = blockComponents[i];
        if (comp == draggedComp) continue;                         // skip self
        if (i >= originalBounds.size()) continue;

        if (originalBounds[i].contains(centre)) {
            auto* targetBlock = comp->getBlock();
            if (targetBlock == nullptr) continue;

            bool sameStack = (draggedBlock->stackGroup >= 0
                           && draggedBlock->stackGroup == targetBlock->stackGroup);

            currentDropAction = sameStack ? DropAction::RearrangeInStack : DropAction::Stack;
            dropTargetIndex   = i;
            dropTargetComp    = comp;
            repaint();
            return;
        }
    }

    // No block hit — find insertion point for reorder.
    // dropTargetIndex = "insert before this block index" (== blockComponents.size() → append).
    currentDropAction = DropAction::Reorder;
    dropTargetIndex   = blockComponents.size();  // default: append
    for (int i = 0; i < originalBounds.size(); ++i) {
        if (blockComponents[i] == draggedComp) continue;
        if (originalBounds[i].getCentreX() > centre.x) {
            dropTargetIndex = i;
            break;
        }
    }
    repaint();
}

void BlockStrip::blockDropped(BlockComponent* draggedComp, juce::Point<int> centre, bool shiftDrag) {
    if (!project) { clearDragFeedback(); return; }

    // Recalculate at the exact release position (mouseDrag isn't called for the final pixel).
    updateDragFeedback(draggedComp, centre, shiftDrag);

    auto* draggedBlock = draggedComp->getBlock();
    if (draggedBlock == nullptr || currentDropAction == DropAction::None) {
        clearDragFeedback();
        return;
    }

    if (currentDropAction == DropAction::Stack) {
        // Stack with a different block / stack group.
        project->stackBlocks(draggedBlock->id, dropTargetComp->getBlock()->id);

    } else if (currentDropAction == DropAction::RearrangeInStack) {
        // Swap the two blocks' positions in project->blocks to change vertical order.
        auto* targetBlock = dropTargetComp->getBlock();
        int draggedIdx = project->blocks.indexOf(draggedBlock);
        int targetIdx  = project->blocks.indexOf(targetBlock);
        if (draggedIdx >= 0 && targetIdx >= 0 && draggedIdx != targetIdx) {
            auto pre = project->toJSON();
            project->blocks.swap(draggedIdx, targetIdx);
            project->applyExternalMutation(pre);
        }

    } else if (currentDropAction == DropAction::Reorder) {
        int fromIndex = project->blocks.indexOf(draggedBlock);
        if (fromIndex < 0) { clearDragFeedback(); return; }

        if (shiftDrag && draggedBlock->stackGroup >= 0) {
            // Shift+drag: move the entire stack as a unit to the new position.
            auto pre = project->toJSON();
            int stackGroup = draggedBlock->stackGroup;

            juce::Array<int> stackIndices;
            for (int i = 0; i < project->blocks.size(); ++i)
                if (project->blocks[i]->stackGroup == stackGroup)
                    stackIndices.add(i);

            // Extract (remove high→low to keep lower indices stable).
            juce::OwnedArray<Block> extracted;
            for (int i = stackIndices.size() - 1; i >= 0; --i)
                extracted.insert(0, project->blocks.removeAndReturn(stackIndices[i]));

            // Adjust insertion point for each removed index below it.
            int insertPos = dropTargetIndex;
            for (int idx : stackIndices)
                if (idx < insertPos) --insertPos;
            insertPos = juce::jlimit(0, project->blocks.size(), insertPos);

            // Re-insert in original order. Capture count before mutating extracted.
            int count = extracted.size();
            for (int i = 0; i < count; ++i)
                project->blocks.insert(insertPos + i, extracted.removeAndReturn(0));

            for (int i = 0; i < project->blocks.size(); ++i)
                project->blocks[i]->position = i;

            project->applyExternalMutation(pre);

        } else if (draggedBlock->stackGroup >= 0) {
            // No Shift: unstack this block and move it to the new position.
            auto pre = project->toJSON();
            int oldGroup = draggedBlock->stackGroup;
            draggedBlock->stackGroup = -1;

            // If only one member remains in the old stack, dissolve it too.
            int remaining = 0;
            Block* lastInStack = nullptr;
            for (auto* b : project->blocks) {
                if (b->stackGroup == oldGroup) { ++remaining; lastInStack = b; }
            }
            if (remaining == 1 && lastInStack != nullptr)
                lastInStack->stackGroup = -1;

            // Move to drop position.
            int insertBefore = juce::jlimit(0, project->blocks.size(), dropTargetIndex);
            int dest;
            if (insertBefore >= project->blocks.size())
                dest = project->blocks.size() - 1;
            else if (fromIndex < insertBefore)
                dest = insertBefore - 1;
            else
                dest = insertBefore;
            dest = juce::jlimit(0, project->blocks.size() - 1, dest);

            if (fromIndex != dest)
                project->blocks.move(fromIndex, dest);

            for (int i = 0; i < project->blocks.size(); ++i)
                project->blocks[i]->position = i;

            project->applyExternalMutation(pre);

        } else {
            // Plain reorder (non-stacked block).
            int insertBefore = juce::jlimit(0, project->blocks.size(), dropTargetIndex);
            int dest;
            if (insertBefore >= project->blocks.size()) {
                dest = project->blocks.size() - 1;
            } else if (fromIndex < insertBefore) {
                dest = insertBefore - 1;
            } else {
                dest = insertBefore;
            }
            dest = juce::jlimit(0, project->blocks.size() - 1, dest);

            if (fromIndex != dest)
                project->moveBlock(fromIndex, dest);
        }
    }

    // Snap sibling stack tiles back to their original positions before the
    // component array is freed by rebuildBlocks().
    {
        juce::HashMap<BlockComponent*, juce::Point<int>>::Iterator it(stackDragStartPositions);
        while (it.next())
            it.getKey()->setTopLeftPosition(it.getValue());
    }
    stackDragStartPositions.clear();

    // activeDragComp must be cleared BEFORE rebuildBlocks() frees the array.
    activeDragComp        = nullptr;
    needsRebuildAfterDrag = false;
    currentDropAction     = DropAction::None;
    dropTargetIndex       = -1;
    dropTargetComp        = nullptr;
    isStackMove           = false;

    rebuildBlocks();
    resized();
    repaint();
}

void BlockStrip::clearDragFeedback() {
    activeDragComp    = nullptr;
    currentDropAction = DropAction::None;
    dropTargetIndex   = -1;
    dropTargetComp    = nullptr;
    isStackMove       = false;
    stackDragStartPositions.clear();
    repaint();
}

void BlockStrip::beginStackDrag(int stackGroup) {
    stackDragStartPositions.clear();
    for (auto* comp : blockComponents) {
        auto* b = comp->getBlock();
        if (b && b->stackGroup == stackGroup)
            stackDragStartPositions.set(comp, comp->getPosition());
    }
    // Bring all stack tiles to front so they render above non-stack tiles.
    // The dragged tile will call toFront(true) immediately after, putting it on top.
    for (auto* comp : blockComponents) {
        auto* b = comp->getBlock();
        if (b && b->stackGroup == stackGroup)
            comp->toFront(false);
    }
}

void BlockStrip::moveStackComponents(int stackGroup, BlockComponent* draggedComp, juce::Point<int> delta) {
    for (auto* comp : blockComponents) {
        if (comp == draggedComp) continue;
        auto* b = comp->getBlock();
        if (b && b->stackGroup == stackGroup && stackDragStartPositions.contains(comp))
            comp->setTopLeftPosition(stackDragStartPositions[comp] + delta);
    }
}

} // namespace BlockShuffler
