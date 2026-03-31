#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Project.h"
#include "../Model/Block.h"
#include "../Model/Clip.h"
#include "LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

class InspectorPanel : public juce::Component,
                       public juce::Slider::Listener,
                       public juce::Button::Listener {
public:
    InspectorPanel();
    ~InspectorPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setProject(Project* p);
    void setClip(Clip* clip, Block* block);
    void setBlock(Block* block);

    /** Refresh displayed values without rebuilding link rows.
     *  Call this when the project changes but the link structure is unchanged. */
    void refreshValues();

    // juce::Slider::Listener
    void sliderValueChanged(juce::Slider* slider) override;
    void sliderDragStarted (juce::Slider* slider) override;
    void sliderDragEnded   (juce::Slider* slider) override;

    // juce::Button::Listener
    void buttonClicked(juce::Button* btn) override;

    /** Called live while the probability slider is dragged so the waveform
     *  view can repaint its effective-probability headers immediately. */
    std::function<void()> onClipProbabilityChanged;

private:
    Project* project      = nullptr;
    Clip*    selectedClip  = nullptr;
    Block*   selectedBlock = nullptr;

    // Clip section
    juce::Label  clipTitle;
    juce::Label  probLabel;
    juce::Slider probSlider;
    juce::Label  effectiveProbLabel;  // shows normalized probability below the slider
    juce::Label  tempoLabel;
    juce::TextEditor tempoField;
    juce::ToggleButton songEnderToggle { "Song Ender" };
    juce::ToggleButton clipDoneToggle  { "Mark as Done" };
    juce::ToggleButton retainLeadIn    { "Retain Lead-In Tempo" };
    juce::ToggleButton retainTail      { "Retain Tail Tempo" };

    // Block section
    juce::Label        blockTitle;
    juce::ToggleButton blockDoneToggle  { "Mark Block as Done" };
    juce::Label        overlapLabel;
    juce::Slider       overlapSlider;

    // Stack controls (visible only when block has a stackGroup >= 0)
    juce::Label        stackPlayCountLabel;
    juce::Label        stackPlayModeLabel;
    juce::ToggleButton simultaneousToggle { "Simultaneous" };

    // Weighted stack-count editor rows (rebuilt when stack group changes)
    struct StackCountRow {
        juce::TextButton decBtn    { "-" };
        juce::Label      countLbl;
        juce::TextButton incBtn    { "+" };
        juce::Slider     weightSlider;
        juce::TextButton removeBtn { "x" };
        juce::var        dragPre;   // pre-snapshot captured at weight-slider drag start

        StackCountRow() {
            weightSlider.setSliderStyle(juce::Slider::LinearHorizontal);
            weightSlider.setRange(1.0, 100.0, 1.0);
            weightSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 18);
        }
    };
    juce::OwnedArray<StackCountRow> stackCountRows;
    juce::TextButton addStackCountBtn { "+ Entry" };
    int lastBuiltStackCountRows = -1;

    // Links section — rebuilt each time a block is selected or link structure changes
    juce::Label  linksTitle;
    struct LinkRow {
        juce::String blockA, blockB;
        juce::Label  label;
        juce::Slider slider;
        juce::var    dragPre;  // pre-snapshot captured at slider drag start
        LinkRow() {
            slider.setSliderStyle(juce::Slider::LinearHorizontal);
            slider.setRange(0.0, 100.0, 1.0);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
        }
    };
    juce::OwnedArray<LinkRow> linkRows;

    // Tracks link count at last rebuild to detect structural changes
    int lastBuiltLinkCount = -1;

    // "Plays Over" section — only visible when the selected block has isOverlapping == true
    juce::Label playsOverTitle;
    juce::Label playsOverHint;
    struct PlaysOverRow {
        juce::ToggleButton toggle { "" };
        juce::String       clipId;
    };
    juce::OwnedArray<PlaysOverRow> playsOverRows;
    int lastBuiltPlaysOverClipCount = -1;

    void rebuildPlaysOverRows();

    bool updatingFromModel = false;

    // Pre-change snapshots captured at drag-start, used in sliderDragEnded to record undo
    juce::var probSliderDragPre;
    juce::var overlapSliderDragPre;

    void updateFromModel();
    void rebuildLinkRows();
    void rebuildStackCountRows();
    BlockLink* findLinkForRow(const LinkRow* row) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InspectorPanel)
};

} // namespace BlockShuffler
