#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include "Block.h"
#include "BlockLink.h"

namespace BlockShuffler {

class Project : public juce::ChangeBroadcaster {
public:
    Project();
    ~Project() override = default;

    // Metadata
    juce::String name;
    double sampleRate = 48000.0;

    // Content
    juce::OwnedArray<Block>     blocks;
    juce::OwnedArray<BlockLink> links;

    // Audio / Undo
    juce::AudioFormatManager formatManager;
    juce::UndoManager        undoManager;

    // Block operations  (each records an undo snapshot automatically)
    Block* addBlock(const juce::String& name = "New Block");
    void   removeBlock(const juce::String& blockId);
    void   moveBlock(int fromIndex, int toIndex);
    void   stackBlocks(const juce::String& blockIdA, const juce::String& blockIdB);
    Block* getBlockById(const juce::String& blockId);

    // Link operations
    BlockLink* addLink(const juce::String& blockA, const juce::String& blockB, float probability = 0.5f);
    void       removeLink(const juce::String& blockA, const juce::String& blockB);
    void       removeLinksForBlock(const juce::String& blockId);
    void       setLinkProbability(const juce::String& blockA, const juce::String& blockB, float prob);

    // Serialization
    juce::var toJSON() const;
    bool      fromJSON(const juce::var& json);
    bool      saveToFile(const juce::File& file);
    bool      loadFromFile(const juce::File& file);

    /**
     * Wipes all blocks/links and reloads from JSON.
     * Used by the undo/redo system — does NOT push a new undo snapshot.
     */
    void resetAndLoad(const juce::var& snapshot);

    /**
     * Records an undo snapshot for a change that was made directly to model objects
     * (e.g. clip probability, block color) without going through a Project method.
     * Also fires sendChangeMessage() to refresh the UI.
     */
    void applyExternalMutation(const juce::var& preSnapshot);

    /** Populated during loadFromFile() — lists audio file paths that were not found. */
    juce::StringArray missingFilesOnLoad;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Project)

private:
    bool suppressUndo = false;
    void recordMutation(const juce::var& preSnapshot);
};

} // namespace BlockShuffler
