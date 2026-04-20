#pragma once
#include <juce_core/juce_core.h>

namespace BlockShuffler {

inline juce::String generateUuid() {
    return juce::Uuid().toString();
}

} // namespace BlockShuffler
