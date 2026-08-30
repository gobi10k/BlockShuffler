#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace BlockShuffler {

class TransportBar : public juce::Component {
public:
    TransportBar();
    ~TransportBar() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Callbacks wired by MainComponent
    std::function<void()> onPlay;
    std::function<void()> onStop;
    std::function<void()> onRewind;
    std::function<void()> onExport;
    std::function<void()> onSave;
    std::function<void()> onSaveAs;
    std::function<void()> onOpen;

    void setIsPlaying(bool playing);
    void setTimeDisplay(double currentSec, double totalSec);

private:
    juce::TextButton rewindBtn { "|<" };
    juce::TextButton playBtn   { "Play" };
    juce::TextButton stopBtn   { "Stop" };
    juce::TextButton exportBtn { "Export" };
    juce::TextButton saveBtn   { "Save" };
    juce::TextButton saveAsBtn { "Save As" };
    juce::TextButton openBtn   { "Open" };

    bool   isPlaying   = false;
    double currentSecs = 0.0;
    double totalSecs   = 0.0;

    juce::Image logoImage;        // loaded once in constructor from BinaryData
    // 12.3: logo pre-scaled ONCE to the draw size with highResamplingQuality.
    // Windows' software renderer aliases the ~28x downscale if done per-paint at
    // the default (medium) quality; Mac's CoreGraphics only masked the problem.
    juce::Image logoImageScaled;  // rebuilt in resized() only when the size changes

    // Rects calculated in resized(), used in paint()
    juce::Rectangle<int> timeDisplayArea;
    juce::Rectangle<int> brandingArea;

    static juce::String formatTime(double seconds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

} // namespace BlockShuffler
