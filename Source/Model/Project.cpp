#include "Project.h"
#include "Serialization.h"

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
    suppressUndo = true;
    blocks.clear();
    links.clear();
    fromJSON(snapshot);
    suppressUndo = false;
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

    // Assign a color from the built-in palette, cycling through it
    static const juce::Colour blockPalette[] = {
        juce::Colour(0xFFCC4444), juce::Colour(0xFFCC8844), juce::Colour(0xFFCCAA44),
        juce::Colour(0xFF44CC44), juce::Colour(0xFF44CCCC), juce::Colour(0xFF4488CC),
        juce::Colour(0xFF8844CC), juce::Colour(0xFFCC44AA)
    };
    block->color = blockPalette[blocks.size() % 8];

    block->position = blocks.size();
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

Block* Project::getBlockById(const juce::String& blockId) {
    for (auto* b : blocks)
        if (b->id == blockId) return b;
    return nullptr;
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
    sendChangeMessage();
    recordMutation(pre);
}

BlockLink* Project::addLink(const juce::String& blockA, const juce::String& blockB, float probability) {
    // Don't snapshot if link already exists (no-op)
    for (auto* link : links)
        if ((link->blockA == blockA && link->blockB == blockB) ||
            (link->blockA == blockB && link->blockB == blockA))
            return link;

    auto pre = toJSON();
    auto link = std::make_unique<BlockLink>(blockA, blockB, probability);
    auto* ptr = link.get();
    links.add(link.release());
    sendChangeMessage();
    recordMutation(pre);
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
    auto json = toJSON();
    auto jsonString = juce::JSON::toString(json);
    return file.replaceWithText(jsonString);
}

bool Project::loadFromFile(const juce::File& file) {
    missingFilesOnLoad.clear();
    auto jsonString = file.loadFileAsString();
    auto json = juce::JSON::parse(jsonString);
    if (json.isVoid()) return false;
    return fromJSON(json);
}

} // namespace BlockShuffler
