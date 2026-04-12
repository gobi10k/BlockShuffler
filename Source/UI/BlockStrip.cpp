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
        selectBlock(newBlock);
    };
    addButton.setTooltip("Add a new block  [Cmd+click to name it]");
    addAndMakeVisible(addButton);

    // Mode label is added last so it renders on top of the viewport/blocks
    modeLabel.setJustificationType(juce::Justification::centred);
    modeLabel.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Italic")));
    modeLabel.setColour(juce::Label::textColourId,
                        juce::Colour(LookAndFeel_BlockShuffler::accentCol));
    modeLabel.setColour(juce::Label::backgroundColourId,
                        juce::Colour(LookAndFeel_BlockShuffler::bgDark).withAlpha(0.85f));
    modeLabel.setInterceptsMouseClicks(false, false);
    modeLabel.setVisible(false);
    addAndMakeVisible(modeLabel);

    rebuildBlocks();
}

void BlockStrip::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(LookAndFeel_BlockShuffler::bgMedium));

    // Linking/stacking mode tint — the text is drawn by modeLabel (a child component
    // added after the viewport, so it renders on top of block tiles).
    if (pendingMode != PendingMode::None) {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.15f));
        g.fillRect(getLocalBounds());
    }

    // Drag visual feedback: insertion indicators
    if (dragSourceIndex >= 0 && dragDropSlot >= 0 && project) {
        int contentX = dragDropSlot * (blockW + blockGap) + blockW / 2;
        int screenX = viewport.getX() + contentX - viewport.getViewPositionX() + padding;

        if (dragIsUnstacking) {
            if (dragOverIndex >= 0) {
                // UNSTACK AND STACK WITH TARGET: show stacking indicator
                g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.85f));
                g.drawLine(float(screenX), float(viewport.getY() + 4),
                           float(screenX), float(viewport.getBottom() - 4), 2.5f);
                g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.9f));
                g.fillRect(screenX - 6, viewport.getY() + 2, 12, 6);
                g.fillRect(screenX - 6, viewport.getBottom() - 8, 12, 6);
                g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
                g.setFont(9.0f);
                g.drawText("STACK", screenX - 20, viewport.getY() + 8, 40, 12,
                           juce::Justification::centred);
            } else {
                // UNSTACK TO EMPTY SLOT: show horizontal insertion line with "unstacking" indicator
                g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentAmber).withAlpha(0.85f));
                g.drawLine(float(screenX), float(viewport.getY() + 4),
                           float(screenX), float(viewport.getBottom() - 4), 2.5f);
                g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentAmber).withAlpha(0.9f));
                g.fillRect(screenX - 6, viewport.getY() + 2, 12, 6);
                g.fillRect(screenX - 6, viewport.getBottom() - 8, 12, 6);
                g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
                g.setFont(9.0f);
                g.drawText("UNSTACK", screenX - 24, viewport.getY() + 8, 48, 12,
                           juce::Justification::centred);
            }
        } else if (dragSourceSlot >= 0 && dragDropSlot == dragSourceSlot && dragOverIndex >= 0) {
            // REORDER WITHIN STACK: show vertical swap indicator
            g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentTeal).withAlpha(0.7f));
            g.setFont(9.0f);
            g.drawText("REORDER", viewport.getX() + 2, viewport.getBottom() - 14, 48, 12,
                       juce::Justification::centred);
        }
    }
}

void BlockStrip::resized() {
    if (!project) return;

    // blockComponents must mirror project->blocks before we index into it.
    // The async changeListenerCallback rebuilds lazily, but resized() can be
    // called synchronously (e.g. on first window layout) before that fires.
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
    // Each slot entry holds the block indices (into project->blocks) that occupy it.
    struct Slot { juce::Array<int> indices; };
    juce::Array<Slot> slots;
    juce::HashMap<int, int> stackGroupToSlot;  // stackGroup → slot index

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

    // Resize cache
    blockCentreXCache.resize(project->blocks.size());

    int x = 0;
    for (auto& slot : slots) {
        int n   = slot.indices.size();
        // Each block in the stack gets an equal share of the height,
        // with a 2-px gap between stacked tiles.
        int totalGaps = (n - 1) * 2;
        int perH      = (areaH - totalGaps) / n;
        int startY    = (areaH - (perH * n + totalGaps)) / 2;

        for (int j = 0; j < n; ++j) {
            int bi  = slot.indices[j];
            int y   = startY + j * (perH + 2);
            if (bi >= 0 && bi < blockComponents.size())
                blockComponents[bi]->setBounds(x, y, blockW, perH);
            if (bi >= 0 && bi < blockCentreXCache.size())
                blockCentreXCache.set(bi, x + blockW / 2);
        }
        x += blockW + blockGap;
    }

    updateOverlay();
}

void BlockStrip::changeListenerCallback(juce::ChangeBroadcaster*) {
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<BlockStrip>(this)] {
        if (safe) { safe->rebuildBlocks(); safe->resized(); safe->repaint(); }
    });
}

void BlockStrip::rebuildBlocks() {
    if (!project) return;
    dragOverIndex = -1;  // stale pointer after rebuild
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
        // Wire undo callbacks so context-menu changes are undoable
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
        bc->onClipDropped = [this](const juce::String& clipId, const juce::String& targetBlockId) {
            if (onClipDropped) onClipDropped(clipId, targetBlockId);
        };
        bc->setSelected(block->id == selectedBlockId);
        bc->setPlaying(block->id == playingBlockId);
        // Highlight potential targets when in link/stack mode
        bool inPendingMode = (pendingMode != PendingMode::None);
        bool isSource = (block->id == pendingBlockId);
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

    // Scroll the viewport so the selected block is visible
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

void BlockStrip::enterLinkMode(const juce::String& fromBlockId) {
    pendingMode    = PendingMode::Link;
    pendingBlockId = fromBlockId;
    // Highlight all other blocks as potential targets
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
    grabKeyboardFocus();  // ensure ESC key reaches this component
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
    grabKeyboardFocus();  // ensure ESC key reaches this component
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
    // Project fires changeMessage → rebuild
}

void BlockStrip::updateOverlay() {
    if (!overlay || !project) return;
    juce::HashMap<juce::String, int> positions;
    for (int i = 0; i < project->blocks.size(); ++i) {
        if (i >= blockCentreXCache.size()) break;
        // Convert contentArea X to overlay-local coords
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
    // Convert to content-area coordinates (undo viewport scroll and padding offset)
    int contentX = localPt.x - viewport.getX() + viewport.getViewPositionX() - padding;
    for (int i = 0; i < blockComponents.size(); ++i) {
        auto bounds = blockComponents[i]->getBounds(); // relative to contentArea
        if (contentX >= bounds.getX() && contentX < bounds.getRight())
            return (i < project->blocks.size()) ? project->blocks[i] : nullptr;
    }
    return nullptr;
}

int BlockStrip::blockCentreX(int blockIndex) const {
    if (blockIndex < 0 || blockIndex >= blockCentreXCache.size()) return 0;
    return viewport.getX() + blockCentreXCache[blockIndex]
           - viewport.getViewPositionX() + padding;
}

bool BlockStrip::isInterestedInDragSource(const SourceDetails& details) {
    return details.description.toString().startsWith("block:");
}

// ── Drag-over helpers ─────────────────────────────────────────────────────────

juce::Point<int> BlockStrip::toContentPos(juce::Point<int> stripLocal) const {
    return { stripLocal.x - viewport.getX() + viewport.getViewPositionX() - padding,
             stripLocal.y - viewport.getY() };
}

int BlockStrip::blockIndexAtContentPos(juce::Point<int> contentPos) const {
    for (int i = 0; i < blockComponents.size(); ++i) {
        const auto bounds = blockComponents[i]->getBounds();
        if (bounds.contains(contentPos))
            return i;
    }
    return -1;
}

void BlockStrip::setDragOver(int newIndex, bool isReorder) {
    if (newIndex == dragOverIndex) {
        if (newIndex >= 0 && newIndex < blockComponents.size())
            blockComponents[newIndex]->setDragTargetMode(isReorder);
        return;
    }
    // Clear old highlight
    if (dragOverIndex >= 0 && dragOverIndex < blockComponents.size())
        blockComponents[dragOverIndex]->setDragTarget(false);
    dragOverIndex = newIndex;
    if (dragOverIndex >= 0 && dragOverIndex < blockComponents.size()) {
        blockComponents[dragOverIndex]->setDragTarget(true);
        blockComponents[dragOverIndex]->setDragTargetMode(isReorder);
    }
}

void BlockStrip::itemDragEnter(const SourceDetails& details) {
    if (!project) return;
    auto descStr = details.description.toString();
    if (!descStr.startsWith("block:")) return;
    auto blockId   = descStr.substring(6);
    if (blockId.isEmpty()) return;
    auto contentPos = toContentPos(details.localPosition);

    dragSourceIndex = -1;
    for (int i = 0; i < project->blocks.size(); ++i) {
        if (project->blocks[i]->id == blockId) { dragSourceIndex = i; break; }
    }
    if (dragSourceIndex < 0) return;

    struct SlotInfo { juce::Array<int> indices; };
    juce::Array<SlotInfo> slots;
    juce::HashMap<int, int> sgToSlot;
    for (int i = 0; i < project->blocks.size(); ++i) {
        int sg = project->blocks[i]->stackGroup;
        if (sg < 0) {
            SlotInfo s; s.indices.add(i); slots.add(std::move(s));
        } else {
            if (sgToSlot.contains(sg)) {
                slots.getReference(sgToSlot[sg]).indices.add(i);
            } else {
                sgToSlot.set(sg, slots.size());
                SlotInfo s; s.indices.add(i); slots.add(std::move(s));
            }
        }
    }

    dragSourceSlot = -1;
    for (int s = 0; s < slots.size(); ++s) {
        if (slots[s].indices.contains(dragSourceIndex)) { dragSourceSlot = s; break; }
    }

    dragIsUnstacking = false;

    int  over = blockIndexAtContentPos(contentPos);
    if (over >= 0 && over < (int)project->blocks.size() &&
        project->blocks[over]->id != blockId)
        setDragOver(over, false);
    else
        setDragOver(-1);

    repaint();
}

void BlockStrip::itemDragMove(const SourceDetails& details) {
    if (!project) return;
    auto descStr = details.description.toString();
    if (!descStr.startsWith("block:")) return;
    auto blockId   = descStr.substring(6);
    if (blockId.isEmpty()) return;
    auto contentPos = toContentPos(details.localPosition);

    struct SlotInfo { juce::Array<int> indices; };
    juce::Array<SlotInfo> slots;
    juce::HashMap<int, int> sgToSlot;
    for (int i = 0; i < project->blocks.size(); ++i) {
        int sg = project->blocks[i]->stackGroup;
        if (sg < 0) {
            SlotInfo s; s.indices.add(i); slots.add(std::move(s));
        } else {
            if (sgToSlot.contains(sg)) {
                slots.getReference(sgToSlot[sg]).indices.add(i);
            } else {
                sgToSlot.set(sg, slots.size());
                SlotInfo s; s.indices.add(i); slots.add(std::move(s));
            }
        }
    }

    int dropSlot = juce::jlimit(0, slots.size() - 1,
                                contentPos.x / (blockW + blockGap));

    dragDropSlot = dropSlot;

    // Determine if we are still inside the source column using actual pixel bounds
    bool sameColumn = false;
    if (dragSourceSlot >= 0) {
        int sourceSlotLeft  = dragSourceSlot * (blockW + blockGap);
        int sourceSlotRight = sourceSlotLeft + blockW;
        sameColumn = (contentPos.x >= sourceSlotLeft && contentPos.x < sourceSlotRight);
    }
    dragIsUnstacking = (dragSourceSlot >= 0 && !sameColumn);

    int  over = blockIndexAtContentPos(contentPos);
    bool isReorder = !dragIsUnstacking;
    if (over >= 0 && over < (int)project->blocks.size() &&
        project->blocks[over]->id != blockId)
        setDragOver(over, isReorder);
    else
        setDragOver(-1);

    repaint();
}

void BlockStrip::itemDragExit(const SourceDetails&) {
    setDragOver(-1);
    dragSourceIndex = -1;
    dragSourceSlot  = -1;
    dragIsUnstacking = false;
    dragDropSlot = -1;
    repaint();
}

void BlockStrip::itemDropped(const SourceDetails& details) {
    setDragOver(-1);
    dragSourceIndex = -1;
    dragSourceSlot  = -1;
    dragIsUnstacking = false;
    repaint();
    if (!project) return;

    auto descStr = details.description.toString();
    if (!descStr.startsWith("block:")) return;
    auto blockId = descStr.substring(6);
    if (blockId.isEmpty()) return;
    int fromIndex = -1;
    for (int i = 0; i < project->blocks.size(); ++i)
        if (project->blocks[i]->id == blockId) { fromIndex = i; break; }
    if (fromIndex < 0) return;

    auto contentPos = toContentPos(details.localPosition);
    int  overIndex  = blockIndexAtContentPos(contentPos);

    struct SlotInfo { juce::Array<int> indices; };
    juce::Array<SlotInfo> slots;
    juce::HashMap<int, int> sgToSlot;
    for (int i = 0; i < project->blocks.size(); ++i) {
        int sg = project->blocks[i]->stackGroup;
        if (sg < 0) {
            SlotInfo s; s.indices.add(i); slots.add(std::move(s));
        } else {
            if (sgToSlot.contains(sg)) {
                slots.getReference(sgToSlot[sg]).indices.add(i);
            } else {
                sgToSlot.set(sg, slots.size());
                SlotInfo s; s.indices.add(i); slots.add(std::move(s));
            }
        }
    }

    int fromSlot = -1;
    for (int s = 0; s < slots.size(); ++s)
        if (slots[s].indices.contains(fromIndex)) { fromSlot = s; break; }

    int dropSlot = juce::jlimit(0, juce::jmax(0, slots.size() - 1),
                                contentPos.x / (blockW + blockGap));

    auto* draggedBlock      = project->blocks[fromIndex];
    bool draggedIsStacked   = (draggedBlock->stackGroup >= 0);
    bool droppedOnDiffBlock = (overIndex >= 0 && overIndex != fromIndex);

    // CASE 3: Stacked block dragged out of its stack column → unstack
    if (draggedIsStacked && dragIsUnstacking) {
        // CASE 3: stacked block dragged to a different horizontal slot → unstack
        auto pre = project->toJSON();

        int oldGroup = draggedBlock->stackGroup;
        draggedBlock->stackGroup = -1;

        int remaining = 0;
        for (auto* b : project->blocks)
            if (b->stackGroup == oldGroup) ++remaining;
        if (remaining <= 1)
            for (auto* b : project->blocks)
                if (b->stackGroup == oldGroup) b->stackGroup = -1;

        if (droppedOnDiffBlock) {
            auto* targetBlock = project->blocks[overIndex];
            if (targetBlock->stackGroup >= 0) {
                draggedBlock->stackGroup = targetBlock->stackGroup;
            } else {
                int maxGroup = -1;
                for (auto* b : project->blocks)
                    maxGroup = juce::jmax(maxGroup, b->stackGroup);
                draggedBlock->stackGroup = maxGroup + 1;
                targetBlock->stackGroup  = maxGroup + 1;
            }
            project->propagateStackSettings(draggedBlock->stackGroup);
        } else {
            // Rebuild slots after unstacking (slot count may have changed)
            struct SlotInfo { juce::Array<int> indices; };
            juce::Array<SlotInfo> newSlots;
            juce::HashMap<int, int> sgToSlot2;
            for (int i = 0; i < project->blocks.size(); ++i) {
                int sg = project->blocks[i]->stackGroup;
                if (sg < 0) {
                    SlotInfo s; s.indices.add(i); newSlots.add(std::move(s));
                } else {
                    if (sgToSlot2.contains(sg)) {
                        newSlots.getReference(sgToSlot2[sg]).indices.add(i);
                    } else {
                        sgToSlot2.set(sg, newSlots.size());
                        SlotInfo s; s.indices.add(i); newSlots.add(std::move(s));
                    }
                }
            }

            // Recalculate dropSlot with new slot count
            int newDropSlot = juce::jlimit(0, juce::jmax(0, newSlots.size() - 1),
                                           contentPos.x / (blockW + blockGap));

            int toIndex = newSlots[newDropSlot].indices[0];
            if (fromIndex != toIndex) {
                project->blocks.move(fromIndex, toIndex);
                for (int i = 0; i < project->blocks.size(); ++i)
                    project->blocks[i]->position = i;
            }
            // Propagate settings to remaining blocks in old stack (if any)
            project->propagateStackSettings(oldGroup);
        }

        project->applyExternalMutation(pre);

    } else if (!draggedIsStacked && droppedOnDiffBlock) {
        // CASE 1: non-stacked block dropped onto another block → stack
        project->stackBlocks(blockId, project->blocks[overIndex]->id);

    } else if (draggedIsStacked && fromSlot >= 0) {
        // CASE 2: stacked block dragged within its stack column (swap or reorder)
        auto pre = project->toJSON();

        // Get all blocks in this stack
        juce::Array<int> slotIndices = slots[fromSlot].indices;
        int fromPos = slotIndices.indexOf(fromIndex);
        int toPos = (overIndex >= 0) ? slotIndices.indexOf(overIndex) : -1;

        if (toPos >= 0 && toPos != fromPos) {
            // Dropped directly on another block → swap positions
            int blockA = slotIndices[fromPos];
            int blockB = slotIndices[toPos];
            project->blocks.swap(blockA, blockB);
        } else if (toPos == -1 && overIndex == -1) {
            // Dropped in a gap → insert at nearest position based on Y
            auto contentPos = toContentPos(details.localPosition);
            int yInStack = contentPos.y;
            int targetPos = slotIndices.size();
            for (int i = 0; i < slotIndices.size(); ++i) {
                auto bounds = blockComponents[slotIndices[i]]->getBounds();
                if (yInStack < bounds.getCentreY()) { targetPos = i; break; }
            }
            if (targetPos != fromPos) {
                auto* draggedBlockPtr = project->blocks[fromIndex];
                project->blocks.remove(fromIndex);
                int newTarget = (targetPos > fromPos) ? targetPos - 1 : targetPos;
                project->blocks.insert(newTarget, draggedBlockPtr);
            }
        }

        // Update position members
        for (int p = 0; p < slotIndices.size(); ++p) {
            int bi = slotIndices[p];
            if (bi >= 0 && bi < project->blocks.size())
                project->blocks[bi]->position = p;
        }

        project->applyExternalMutation(pre);

    } else {
        // Plain horizontal reorder (non-stacked block to new position, or no-op)
        if (dropSlot >= 0 && dropSlot < slots.size() && !slots[dropSlot].indices.isEmpty()) {
            int toIndex = slots[dropSlot].indices[0];
            if (fromIndex != toIndex)
                project->moveBlock(fromIndex, toIndex);
        }
    }
}

} // namespace BlockShuffler
