#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Project.h"

namespace BlockShuffler {
class BlockStackView : public juce::Component {
public:
    BlockStackView() = default;
    ~BlockStackView() override = default;
    void paint(juce::Graphics& g) override;
    void resized() override;
};
} // namespace BlockShuffler
