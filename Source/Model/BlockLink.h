#pragma once
#include <juce_core/juce_core.h>

namespace BlockShuffler {

struct BlockLink {
    juce::String blockA;        // UUID of first block
    juce::String blockB;        // UUID of second block
    float swapProbability;      // 0.0 – 1.0

    BlockLink() : swapProbability(0.5f) {}
    BlockLink(const juce::String& a, const juce::String& b, float prob = 0.5f)
        : blockA(a), blockB(b), swapProbability(prob) {}
};

} // namespace BlockShuffler
