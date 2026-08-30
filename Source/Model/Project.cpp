#include "Project.h"
#include "Serialization.h"
#include "../UI/LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

//==============================================================================
// Snapshot-based undo action
//==============================================================================
class SnapshotAction : public juce::UndoableAction {
public:
    SnapshotAction(Project& p, juce::var before_, juce::var after_)
        : project(p), before(std::move(before_)), after(std::move(after_)) {}

    bool perform() override {
        // On first perform() the mutation has already been applied by the calling
        // code, so we skip re-applying it.  Subsequent calls (redo) do need to
        // replay it.
        if (firstPerform) { firstPerform = false; return true; }
        project.resetAndLoad(after);
        return true;
    }

    bool undo() override {
        project.resetAndLoad(before);
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    Project&   project;
    juce::var  before, after;
    bool       firstPerform = true;
};

//==============================================================================
Project::Project() {
    formatManager.registerBasicFormats(); // WAV, AIFF, FLAC, OGG
}

void Project::recordMutation(const juce::var& preSnapshot) {
    if (suppressUndo) return;
    // Each recordMutation call is its own transaction.
    // Without this, all actions accumulate in one transaction and undo() reverts all of them.
    undoManager.beginNewTransaction();
    undoManager.perform(new SnapshotAction(*this, preSnapshot, toJSON()));
}

void Project::applyExternalMutation(const juce::var& preSnapshot) {
    sendChangeMessage();
    recordMutation(preSnapshot);
}

void Project::resetAndLoad(const juce::var& snapshot) {
    struct Guard {
        Project& p;
        Guard(Project& proj) : p(proj) { p.suppressUndo = true; }
        ~Guard() { p.suppressUndo = false; }
    } guard(*this);

    blocks.clear();
    links.clear();
    fromJSON(snapshot);
    sendChangeMessage();
}

//==============================================================================
Block* Project::addBlock(const juce::String& blockName) {
    auto pre = toJSON();
    auto block = std::make_unique<Block>();

    // Auto-generate unique name if none given (or if using the generic default)
    if (blockName.isEmpty() || blockName == "New Block")
        block->name = "Block " + juce::String(blocks.size() + 1);
    else
        block->name = blockName;

    // Assign a color from the brand accent palette, cycling through it
    auto palette = LookAndFeel_BlockShuffler::getBlockPalette();
    block->color = palette[blocks.size() % palette.size()];

    block->position = blocks.isEmpty() ? 0 : blocks.getLast()->position + 1;

    // New blocks adopt the project default tempo (same fallback 9.4 uses for clips),
    // so clips added to them inherit it via the block-tempo-priority paths (9.3).
    block->tempo = defaultClipTempo > 0.0 ? defaultClipTempo : 120.0;

    auto* ptr = block.get();
    blocks.add(block.release());
    sendChangeMessage();
    recordMutation(pre);
    return ptr;
}

void Project::removeBlock(const juce::String& blockId) {
    auto pre = toJSON();
    for (int i = 0; i < blocks.size(); ++i) {
        if (blocks[i]->id == blockId) {
            blocks.remove(i);
            for (int j = 0; j < blocks.size(); ++j)
                blocks[j]->position = j;
            // Prune links referencing the deleted block inside this same
            // snapshot — NOT via removeLinksForBlock(), which would record a
            // second undo transaction. `pre` already captured the links, so
            // one undo restores the block and its links together.
            for (int k = links.size() - 1; k >= 0; --k)
                if (links[k]->blockA == blockId || links[k]->blockB == blockId)
                    links.remove(k);
            sendChangeMessage();
            recordMutation(pre);
            return;
        }
    }
}

void Project::moveBlock(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= blocks.size()) return;
    if (toIndex < 0 || toIndex >= blocks.size()) return;
    auto pre = toJSON();
    blocks.move(fromIndex, toIndex);
    for (int i = 0; i < blocks.size(); ++i)
        blocks[i]->position = i;
    sendChangeMessage();
    recordMutation(pre);
}

void Project::propagateStackSettings(int stackGroup, Block* sourceBlock) {
    if (stackGroup < 0) return;
    Block* source = sourceBlock;
    if (source == nullptr || source->stackGroup != stackGroup) {
        for (auto* b : blocks) {
            if (b->stackGroup == stackGroup) { source = b; break; }
        }
    }
    if (source == nullptr || source->stackGroup != stackGroup) return;
    for (auto* b : blocks) {
        if (b->stackGroup == stackGroup && b != source) {
            b->stackPlayCount  = source->stackPlayCount;
            b->stackPlayMode   = source->stackPlayMode;
            b->alwaysPlayBase  = source->alwaysPlayBase;
        }
    }
}

Block* Project::getBlockById(const juce::String& blockId) {
    for (auto* b : blocks)
        if (b->id == blockId) return b;
    return nullptr;
}

//==============================================================================
// Tempo inherit/override write-paths. Values are propagated at write time so
// every read path keeps seeing a materialized tempo; the flags only gate which
// targets a propagation may touch.
void Project::setClipTempo(Clip& clip, double t) {
    if (t <= 0.0) return;
    auto pre = toJSON();
    clip.tempo = t;
    clip.tempoOverridden = true;
    sendChangeMessage();
    recordMutation(pre);
}

void Project::setBlockTempo(Block& block, double t) {
    if (t <= 0.0) return;
    auto pre = toJSON();
    block.tempo = t;
    block.tempoOverridden = true;
    for (auto* c : block.clips)
        if (!c->tempoOverridden) c->tempo = t;
    sendChangeMessage();
    recordMutation(pre);
}

void Project::setDefaultTempo(double t) {
    if (t <= 0.0) return;
    auto pre = toJSON();
    defaultClipTempo = t;
    for (auto* b : blocks) {
        if (b->tempoOverridden) continue;   // overridden block shields ALL its clips
        b->tempo = t;
        for (auto* c : b->clips)
            if (!c->tempoOverridden) c->tempo = t;
    }
    sendChangeMessage();
    recordMutation(pre);
}

void Project::resetClipTempoToInherited(Clip& clip, Block& block) {
    if (!clip.tempoOverridden) return;      // already inheriting — no undo entry
    auto pre = toJSON();
    clip.tempoOverridden = false;
    clip.tempo = block.tempo;
    sendChangeMessage();
    recordMutation(pre);
}

void Project::resetBlockTempoToInherited(Block& block) {
    if (!block.tempoOverridden) return;     // already inheriting — no undo entry
    auto pre = toJSON();
    // Same >0-else-120 fallback addBlock uses for the project default.
    const double t = defaultClipTempo > 0.0 ? defaultClipTempo : 120.0;
    block.tempoOverridden = false;
    block.tempo = t;
    for (auto* c : block.clips)
        if (!c->tempoOverridden) c->tempo = t;
    sendChangeMessage();
    recordMutation(pre);
}

void Project::stackBlocks(const juce::String& blockIdA, const juce::String& blockIdB) {
    auto* a = getBlockById(blockIdA);
    auto* b = getBlockById(blockIdB);
    if (a == nullptr || b == nullptr) return;

    auto pre = toJSON();
    if (a->stackGroup >= 0) {
        b->stackGroup = a->stackGroup;
    } else if (b->stackGroup >= 0) {
        a->stackGroup = b->stackGroup;
    } else {
        int maxGroup = -1;
        for (auto* block : blocks)
            maxGroup = juce::jmax(maxGroup, block->stackGroup);
        a->stackGroup = maxGroup + 1;
        b->stackGroup = maxGroup + 1;
    }
    propagateStackSettings(a->stackGroup);
    sendChangeMessage();
    recordMutation(pre);
}

void Project::detachBlockFromStack(Block& block) {
    const int oldGroup = block.stackGroup;
    if (oldGroup < 0) return;
    block.stackGroup = -1;

    // If only one member remains in the old stack, dissolve it too.
    int remaining = 0;
    Block* lastInStack = nullptr;
    for (auto* b : blocks) {
        if (b->stackGroup == oldGroup) { ++remaining; lastInStack = b; }
    }
    if (remaining == 1 && lastInStack != nullptr) {
        // FIX H6/H7: reset stack settings when dissolving a solo stack
        lastInStack->stackGroup = -1;
        lastInStack->stackPlayCount.values.clearQuick();
        lastInStack->stackPlayCount.values.add(1);
        lastInStack->stackPlayCount.weights.clearQuick();
        lastInStack->stackPlayCount.weights.add(1.0f);
        lastInStack->stackPlayMode = StackPlayMode::Sequential;
    } else if (remaining > 1) {
        propagateStackSettings(oldGroup);
    }
}

void Project::restackBlockOnto(const juce::String& draggedBlockId, const juce::String& targetBlockId) {
    auto* dragged = getBlockById(draggedBlockId);
    auto* target  = getBlockById(targetBlockId);
    if (dragged == nullptr || target == nullptr || dragged == target) return;
    // Same stack = vertical rearrange, not a restack — handled by the caller.
    if (dragged->stackGroup >= 0 && dragged->stackGroup == target->stackGroup) return;

    auto pre = toJSON();

    detachBlockFromStack(*dragged);

    // Join the target's stack, or form a fresh two-block stack with it.
    int group = target->stackGroup;
    if (group < 0) {
        int maxGroup = -1;
        for (auto* b : blocks)
            maxGroup = juce::jmax(maxGroup, b->stackGroup);
        group = maxGroup + 1;
        target->stackGroup = group;
    }
    dragged->stackGroup = group;

    // A stack renders at the slot of its FIRST member in blocks[] order
    // (BlockStrip::resized() scans in array order), so the merged stack must be
    // anchored on the target: reinsert the dragged block directly after the
    // target group's last member. Leaving it at its old index would drag the
    // whole stack back to the dragged block's old slot.
    int from = blocks.indexOf(dragged);
    if (from >= 0) {
        auto* moved = blocks.removeAndReturn(from);
        int lastOfGroup = -1;
        for (int i = 0; i < blocks.size(); ++i)
            if (blocks[i]->stackGroup == group) lastOfGroup = i;
        blocks.insert(lastOfGroup + 1, moved);
    }
    for (int i = 0; i < blocks.size(); ++i)
        blocks[i]->position = i;

    // Target group's settings win (its first member is the propagation source).
    propagateStackSettings(group);
    sendChangeMessage();
    recordMutation(pre);
}

BlockLink* Project::addLink(const juce::String& blockA, const juce::String& blockB, float probability, bool sendNotification) {
    // Don't snapshot if link already exists (no-op)
    for (auto* link : links)
        if ((link->blockA == blockA && link->blockB == blockB) ||
            (link->blockA == blockB && link->blockB == blockA))
            return link;

    juce::var pre;
    if (sendNotification)
        pre = toJSON();
    auto link = std::make_unique<BlockLink>(blockA, blockB, probability);
    auto* ptr = link.get();
    links.add(link.release());
    if (sendNotification) {
        sendChangeMessage();
        recordMutation(pre);
    }
    return ptr;
}

void Project::removeLink(const juce::String& blockA, const juce::String& blockB) {
    for (int i = 0; i < links.size(); ++i) {
        auto* link = links[i];
        if ((link->blockA == blockA && link->blockB == blockB) ||
            (link->blockA == blockB && link->blockB == blockA)) {
            auto pre = toJSON();
            links.remove(i);
            sendChangeMessage();
            recordMutation(pre);
            return;
        }
    }
}

void Project::removeLinksForBlock(const juce::String& blockId) {
    bool anyFound = false;
    for (auto* link : links)
        if (link->blockA == blockId || link->blockB == blockId) { anyFound = true; break; }
    if (!anyFound) return;

    auto pre = toJSON();
    for (int i = links.size() - 1; i >= 0; --i) {
        auto* link = links[i];
        if (link->blockA == blockId || link->blockB == blockId)
            links.remove(i);
    }
    sendChangeMessage();
    recordMutation(pre);
}

void Project::setLinkProbability(const juce::String& blockA, const juce::String& blockB, float prob) {
    for (auto* link : links) {
        if ((link->blockA == blockA && link->blockB == blockB) ||
            (link->blockA == blockB && link->blockB == blockA)) {
            auto pre = toJSON();
            link->swapProbability = juce::jlimit(0.0f, 1.0f, prob);
            sendChangeMessage();
            recordMutation(pre);
            return;
        }
    }
}

//==============================================================================
juce::var Project::toJSON() const {
    return Serialization::projectToJSON(*this);
}

bool Project::fromJSON(const juce::var& json) {
    return Serialization::projectFromJSON(json, *this);
}

bool Project::saveToFile(const juce::File& file) {
    // Pass project directory so audio paths are stored relative to the project file.
    auto json = Serialization::projectToJSON(*this, file.getParentDirectory());
    auto jsonString = juce::JSON::toString(json);
    return file.replaceWithText(jsonString);
}

bool Project::loadFromFile(const juce::File& file) {
    missingFilesOnLoad.clear();
    auto jsonString = file.loadFileAsString();
    auto json = juce::JSON::parse(jsonString);
    if (json.isVoid()) return false;
    // Pass project directory so relative audio paths resolve correctly on any platform.
    return Serialization::projectFromJSON(json, *this, file.getParentDirectory());
}

} // namespace BlockShuffler
