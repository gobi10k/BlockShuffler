#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Project.h"
#include "../Model/Block.h"
#include "../Model/Clip.h"
#include "LookAndFeel_BlockShuffler.h"
#include <map>

namespace BlockShuffler {

// A number box that supports text input (single click) and click-drag to change value.
// Single click opens an inline TextEditor in-place — no popup window.
class DraggableNumberBox : public juce::Component {
public:
    DraggableNumberBox(double rMin = 0.0, double rMax = 1000.0, int decimals = 1)
        : rMin(rMin), rMax(rMax), decimals(decimals)
    {
        addChildComponent(inlineEditor);
        inlineEditor.setInputRestrictions(8, "0123456789.");
        inlineEditor.setJustification(juce::Justification::centred);
        inlineEditor.setSelectAllWhenFocused(true);
        inlineEditor.setFont(LookAndFeel_BlockShuffler::monoFont(13.0f));
        inlineEditor.setColour(juce::TextEditor::backgroundColourId,
                               juce::Colour(LookAndFeel_BlockShuffler::bgLight));
        inlineEditor.setColour(juce::TextEditor::textColourId,
                               juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
        inlineEditor.setColour(juce::TextEditor::outlineColourId,
                               juce::Colour(LookAndFeel_BlockShuffler::accentCol));
        inlineEditor.setColour(juce::TextEditor::focusedOutlineColourId,
                               juce::Colour(LookAndFeel_BlockShuffler::accentCol));

        inlineEditor.onReturnKey  = [this]() { commitEdit(); };
        inlineEditor.onEscapeKey  = [this]() { cancelEdit(); };
        inlineEditor.onFocusLost  = [this]() { commitEdit(); };
    }

    void setValue(double v, juce::NotificationType notify = juce::dontSendNotification) {
        v = juce::jlimit(rMin, rMax, v);
        if (!juce::approximatelyEqual(value, v)) {
            value = v;
            repaint();
            if (notify != juce::dontSendNotification)
                onValueChanged(value);
        }
    }

    double getValue() const { return value; }
    void setTooltip(const juce::String& tip) { tooltip = tip; }

    std::function<void(double)> onValueChanged;

private:
    double value = 0.0;
    double rMin, rMax;
    int decimals;
    juce::String tooltip;
    bool startDragging = false;
    int dragStartY = 0;
    double dragStartValue = 0.0;
    bool editing = false;
    juce::TextEditor inlineEditor;

    void paint(juce::Graphics& g) override {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::bgLight));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
        if (!editing) {
            g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
            g.setFont(LookAndFeel_BlockShuffler::monoFont(13.0f));
            g.drawFittedText(juce::String(value, decimals), getLocalBounds().withTrimmedRight(6),
                             juce::Justification::centredRight, 1);
        }
    }

    void resized() override {
        inlineEditor.setBounds(getLocalBounds());
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (editing) return;
        startDragging = false;
        dragStartY = e.getPosition().y;
        dragStartValue = value;
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (editing) return;
        int delta = std::abs(e.getPosition().y - dragStartY);
        if (!startDragging && delta > 4)
            startDragging = true;
        if (startDragging) {
            int dragDelta = dragStartY - e.getPosition().y;
            double newValue = dragStartValue + dragDelta * 1.0;
            newValue = juce::roundToInt(newValue * 10.0) / 10.0;
            newValue = juce::jlimit(rMin, rMax, newValue);
            setValue(newValue, juce::sendNotification);
        }
    }

    void mouseUp(const juce::MouseEvent&) override {
        if (editing) return;
        if (!startDragging)
            openInlineEditor();
        startDragging = false;
    }

    void openInlineEditor() {
        if (editing) return;
        editing = true;
        inlineEditor.setText(juce::String(value, decimals), false);
        inlineEditor.setVisible(true);
        inlineEditor.grabKeyboardFocus();
        inlineEditor.selectAll();
        repaint();
    }

    void commitEdit() {
        if (!editing) return;
        double newVal = inlineEditor.getText().getDoubleValue();
        newVal = juce::jlimit(rMin, rMax, newVal);
        editing = false;
        inlineEditor.setVisible(false);
        repaint();
        setValue(newVal, juce::sendNotification);
    }

    void cancelEdit() {
        if (!editing) return;
        editing = false;
        inlineEditor.setVisible(false);
        repaint();
    }
};

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

    /** Height needed to lay out all current content (mirrors the removeFromTop
     *  accounting in resized() — keep the two in sync). The panel lives in a
     *  Viewport, so content taller than the window scrolls instead of getting
     *  zero-height bounds from an exhausted layout area (5.12, >12 stack rows). */
    int preferredHeight() const;

    /** Height floor supplied by MainComponent (window-derived). The panel never
     *  shrinks below this even when content is short. */
    void setMinHeight(int h) { minPanelHeight = h; }

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

    /** Guarded resize to jmax(preferredHeight(), minPanelHeight): only calls
     *  setSize when the needed height differs (setSize fires resized(); resized()
     *  never calls back here, so no recursion). Falls back to a plain resized()
     *  when the height is already right, so callers always get a layout pass. */
    void updatePanelHeight();
    int  minPanelHeight = 800;

    // ── Clip section ─────────────────────────────────────────────────────────
    juce::Label  clipTitle;
    juce::Label  probLabel;
    juce::Slider probSlider;
    juce::Label  effectiveProbLabel;
    juce::Label  tempoLabel;
    DraggableNumberBox tempoField { 20.0, 300.0, 1 };
    // U+21BA anticlockwise arrow — must be UTF-8-decoded, plain char* mojibakes
    juce::TextButton clipTempoResetBtn { juce::String(juce::CharPointer_UTF8("\xe2\x86\xba")) };
    juce::ToggleButton songEnderToggle { "Song Ender" };
    juce::ToggleButton clipDoneToggle  { "Mark as Done" };
    juce::ToggleButton retainLeadIn    { "Retain Lead-In Tempo" };
    juce::ToggleButton retainTail      { "Retain Tail Tempo" };

    // ── Block section ────────────────────────────────────────────────────────
    juce::Label        blockTitle;
    juce::Label        blockTempoLabel;
    DraggableNumberBox blockTempoField  { 20.0, 300.0, 1 };
    juce::TextButton   blockTempoResetBtn { juce::String(juce::CharPointer_UTF8("\xe2\x86\xba")) };
    juce::ToggleButton blockDoneToggle  { "Mark Block as Done" };
    juce::Label        playChanceLabel;
    juce::Slider       playChanceSlider;
    juce::var          playChanceSliderDragPre;

    // ── Project section (shown when no block selected) ───────────────────────
    juce::Label        projectTitle;
    juce::Label        defaultTempoLabel;
    DraggableNumberBox defaultTempoField { 20.0, 300.0, 1 };

    // ── Stack settings section (only when block->stackGroup >= 0) ────────────
    juce::Label    stackSectionTitle;   ///< "STACK SETTINGS" heading
    juce::Label    stackInfoLabel;      ///< "Group 1  ·  3 blocks"
    juce::Label    playModeLabel;       ///< "Play Mode:"
    juce::ComboBox playModeCombo;       ///< Sequential / Simultaneous
    juce::ToggleButton alwaysPlayBaseToggle { "Always play base block" };  ///< simultaneous mode only
    juce::Label    stackPlayCountLabel; ///< "How Many to Play:"

    // One row per count entry: [-] [Play X] [+] [x]
    struct StackCountRow {
        juce::TextButton decBtn    { "-" };
        juce::Label      countLbl;
        juce::TextButton incBtn    { "+" };
        juce::TextButton removeBtn { "x" };
    };
    juce::OwnedArray<StackCountRow> stackCountRows;
    juce::TextButton addStackCountBtn { "+ Add" };
    int lastBuiltStackCountRows = -1;

    juce::Label stackBlocksTitle;                 ///< "Blocks in stack:"

    // One row per block in the stack: [name] [====playChance slider====] [eff: XX%]
    struct StackBlockProbRow {
        juce::Label  nameLabel;
        juce::Slider probSlider;
        juce::Label  effectiveLabel;
        StackBlockProbRow() {
            probSlider.setSliderStyle(juce::Slider::LinearHorizontal);
            probSlider.setRange(0.0, 100.0, 1.0);
            probSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 16);
        }
    };
    juce::OwnedArray<StackBlockProbRow> stackBlockProbRows;
    juce::var stackBlockDragPre;
    int lastBuiltStackGroup      = -2;            ///< detect group changes for block list
    bool lastBuiltSimMode        = false;         ///< detect mode flips (rebuilds "(base)" suffix + toggle layout)

    // Track Y+H of the stack section for paint() tint
    int stackSectionY = -1;
    int stackSectionH = 0;

    void rebuildStackCountRows();
    void rebuildStackBlockLabels();
    void recalcStackEffectiveLabels();
    std::map<juce::String, float> computeStackInclusionProbabilities(int stackGroup) const;

    /** Effective-% recompute gate (MASTER_PROMPT 4C): the Monte Carlo runs only
     *  when the stack state it depends on changes — weight drag end, play-count
     *  change, base toggle, stack membership change, project load (and undo of
     *  those). Play / playing-block follow don't touch the model, so they hit
     *  the cache and the displayed % cannot change when playback starts. */
    juce::String stackStateFingerprint(int stackGroup) const;
    struct InclusionCacheEntry {
        juce::String fingerprint;
        std::map<juce::String, float> probs;
    };
    std::map<int, InclusionCacheEntry> inclusionProbsCache;  // keyed by stackGroup

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

    void updateFromModel();
    void rebuildLinkRows();
    BlockLink* findLinkForRow(const LinkRow* row) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InspectorPanel)
};

} // namespace BlockShuffler
