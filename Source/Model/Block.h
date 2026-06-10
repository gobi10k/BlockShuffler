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
          probability(1.0f),
          isDone(false),
          playChance(1.0f),
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

    float probability;     // weight for random selection when in a stack

    // Clips
    juce::OwnedArray<Clip> clips;

    // Flags
    bool isDone;
    float playChance;  // 0.0–1.0, probability that this block is included in the arrangement

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

    const Clip* getClipById(const juce::String& clipId) const {
        for (const auto* c : clips)
            if (c->id == clipId) return c;
        return nullptr;
    }

    Clip* getClipById(const juce::String& clipId) {
        for (auto* c : clips)
            if (c->id == clipId) return c;
        return nullptr;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Block)
    JUCE_DECLARE_WEAK_REFERENCEABLE(Block)
};

} // namespace BlockShuffler
