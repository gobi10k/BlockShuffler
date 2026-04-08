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

    // ── Clip section ─────────────────────────────────────────────────────────
    juce::Label  clipTitle;
    juce::Label  probLabel;
    juce::Slider probSlider;
    juce::Label  effectiveProbLabel;
    juce::Label  tempoLabel;
    juce::TextEditor tempoField;
    juce::ToggleButton songEnderToggle { "Song Ender" };
    juce::ToggleButton clipDoneToggle  { "Mark as Done" };
    juce::ToggleButton retainLeadIn    { "Retain Lead-In Tempo" };
    juce::ToggleButton retainTail      { "Retain Tail Tempo" };

    // ── Block section ────────────────────────────────────────────────────────
    juce::Label        blockTitle;
    juce::ToggleButton blockDoneToggle  { "Mark Block as Done" };
    juce::Label        overlapLabel;
    juce::Slider       overlapSlider;

    // ── "Plays Over" section ─────────────────────────────────────────────────
    juce::Label playsOverTitle;
    juce::Label playsOverHint;
    struct PlaysOverRow {
        juce::ToggleButton toggle { "" };
        juce::String       clipId;
    };
    juce::OwnedArray<PlaysOverRow> playsOverRows;
    int lastBuiltPlaysOverClipCount = -1;

    void rebuildPlaysOverRows();

    // ── Stack settings section (only when block->stackGroup >= 0) ────────────
    juce::Label    stackSectionTitle;   ///< "STACK SETTINGS" heading
    juce::Label    stackInfoLabel;      ///< "Group 1  ·  3 blocks"
    juce::Label    playModeLabel;       ///< "Play Mode:"
    juce::ComboBox playModeCombo;       ///< Sequential / Simultaneous
    juce::Label    stackPlayCountLabel; ///< "How Many to Play:"

    // One row per count entry: [– count +] [weight slider] [×]
    struct StackCountRow {
        juce::TextButton decBtn    { "-" };
        juce::Label      countLbl;
        juce::TextButton incBtn    { "+" };
        juce::Slider     weightSlider;
        juce::TextButton removeBtn { "x" };
        juce::var        dragPre;

        StackCountRow() {
            weightSlider.setSliderStyle(juce::Slider::LinearHorizontal);
            weightSlider.setRange(1.0, 100.0, 1.0);
            weightSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 18);
        }
    };
    juce::OwnedArray<StackCountRow> stackCountRows;
    juce::TextButton addStackCountBtn { "+ Add" };
    int lastBuiltStackCountRows = -1;

    juce::Label stackBlocksTitle;                 ///< "Blocks in stack:"
    juce::OwnedArray<juce::Label> stackBlockLabels; ///< one label per block in group
    int lastBuiltStackGroup      = -2;            ///< detect group changes for block list

    // Track Y+H of the stack section for paint() tint
    int stackSectionY = -1;
    int stackSectionH = 0;

    void rebuildStackCountRows();
    void rebuildStackBlockLabels();

    // ── Links section ────────────────────────────────────────────────────────
    juce::Label  linksTitle;
    struct LinkRow {
        juce::String blockA, blockB;
        juce::Label  label;
        juce::Slider slider;
        juce::var    dragPre;
        LinkRow() {
            slider.setSliderStyle(juce::Slider::LinearHorizontal);
            slider.setRange(0.0, 100.0, 1.0);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
        }
    };
    juce::OwnedArray<LinkRow> linkRows;
    int lastBuiltLinkCount = -1;

    bool updatingFromModel = false;

    juce::var probSliderDragPre;
    juce::var overlapSliderDragPre;

    void updateFromModel();
    void rebuildLinkRows();
    BlockLink* findLinkForRow(const LinkRow* row) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InspectorPanel)
};

} // namespace BlockShuffler
