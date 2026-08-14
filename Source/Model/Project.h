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
    double sampleRate        = 48000.0;
    double defaultClipTempo  = 120.0;

    /** RAWGAIN: raw-summing mode. When true the mixer applies NO crossfade
     *  ramps, no complementary join law and no per-entry stack attenuation —
     *  every entry sums at unity gain, lead-ins and tails included. Entry
     *  timing, lengths and timelinePos are unaffected either way.
     *  Default FALSE: existing sessions keep the fade law byte-identically. */
    bool unityGainMode = false;

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

    /** Drop of an already-stacked block onto another block: detaches it from its
     *  current stack first (so the target is never absorbed into the old stack),
     *  then stacks it with the target — joining the target's stack if it has one,
     *  else forming a fresh two-block stack. The resulting stack is anchored at
     *  the DROP TARGET's slot: the dragged block is reinserted after the target
     *  group's last member, never left at its old index.
     *  One undo entry, one change message. */
    void   restackBlockOnto(const juce::String& draggedBlockId, const juce::String& targetBlockId);

    /** Removes the block from its stack group; if exactly one member remains in
     *  the old group it auto-unstacks with its stack settings reset (H6/H7).
     *  Does NOT send a change message or record undo — callers wrap it. */
    void   detachBlockFromStack(Block& block);

    Block* getBlockById(const juce::String& blockId);

    // Tempo inherit/override write-paths. Tempos stay MATERIALIZED: these
    // setters propagate downward at write time; read paths (resolver, grid,
    // nudge, inspector reads) are untouched. Each call = ONE undo entry.
    void setClipTempo(Clip& clip, double t);            // marks the clip overridden
    void setBlockTempo(Block& block, double t);         // marks the block overridden; skips overridden clips
    void setDefaultTempo(double t);                     // skips overridden blocks (and their clips entirely)
    void resetClipTempoToInherited(Clip& clip, Block& block);
    void resetBlockTempoToInherited(Block& block);

    /** Copies stackPlayCount and stackPlayMode from sourceBlock to all other blocks in the group.
     *  If sourceBlock is null or not in the group, uses the first block as source.
     *  Call this after any mutation to stack-level settings, and after stacking/unstacking. */
    void propagateStackSettings(int stackGroup, Block* sourceBlock = nullptr);

    // Link operations
    BlockLink* addLink(const juce::String& blockA, const juce::String& blockB, float probability = 0.5f, bool sendNotification = true);
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
