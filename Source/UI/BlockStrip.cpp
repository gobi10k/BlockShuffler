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

void BlockStrip::itemDropped(const SourceDetails& details) {
    if (!project) return;
    auto blockId = details.description.toString().substring(6);

    int fromIndex = -1;
    for (int i = 0; i < project->blocks.size(); ++i)
        if (project->blocks[i]->id == blockId) { fromIndex = i; break; }
    if (fromIndex < 0) return;

    int localX   = details.localPosition.x;
    int scrollX  = viewport.getViewPositionX();
    int contentX = localX - viewport.getX() + scrollX - padding;
    int toIndex  = juce::jlimit(0, project->blocks.size() - 1,
                                contentX / (blockW + blockGap));

    if (fromIndex != toIndex)
        project->moveBlock(fromIndex, toIndex);
}

} // namespace BlockShuffler
