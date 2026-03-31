#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include "Clip.h"
#include "../Utils/WeightedRandom.h"
#include "../Utils/UuidGenerator.h"

namespace BlockShuffler {

enum class StackPlayMode { Sequential, Simultaneous };

class Block : public juce::ChangeBroadcaster {
public:
    Block()
        : id(generateUuid()),
          name("New Block"),
          color(juce::Colour(0xFF5599FF)),
          position(0),
          stackGroup(-1),
          stackPlayMode(StackPlayMode::Sequential),
          isOverlapping(false),
          overlapProbability(0.5f),
          isDone(false),
          tempo(120.0) {
        stackPlayCount.values.add(1);
        stackPlayCount.weights.add(1.0f);
    }

    // Identity
    juce::String id;
    juce::String name;
    juce::Colour color;

    // Arrangement
    int position;           // horizontal slot index
    int stackGroup;         // -1 = not stacked; same value = same stack
    WeightedValue<int> stackPlayCount;
    StackPlayMode stackPlayMode;

    // Overlap
    bool isOverlapping;
    float overlapProbability;
    // If non-empty, this overlapping block only plays when the selected parent clip
    // is in this list. Empty = play over any parent clip (default / backward-compatible).
    juce::StringArray allowedParentClipIds;

    // Clips
    juce::OwnedArray<Clip> clips;

    // Flags
    bool isDone;

    // Tempo
    double tempo;

    // Helpers
    void addClip(std::unique_ptr<Clip> clip) {
        clips.add(clip.release());
        sendChangeMessage();
    }

    void removeClip(int index) {
        clips.remove(index);
        sendChangeMessage();
    }

    Clip* getClipById(const juce::String& clipId) {
        for (auto* c : clips)
            if (c->id == clipId) return c;
        return nullptr;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Block)
};

} // namespace BlockShuffler
