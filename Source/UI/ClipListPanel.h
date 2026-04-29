#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Project.h"

namespace BlockShuffler {
class ClipListPanel : public juce::Component {
public:
    ClipListPanel() = default;
    ~ClipListPanel() override = default;
    void paint(juce::Graphics& g) override;
    void resized() override;
};
} // namespace BlockShuffler
