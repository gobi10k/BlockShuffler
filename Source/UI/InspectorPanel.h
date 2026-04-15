#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Project.h"
#include "../Model/Block.h"
#include "../Model/Clip.h"
#include "LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

// A number box that supports text input and click-drag to change value
class DraggableNumberBox : public juce::Component {
public:
    DraggableNumberBox(double rMin = 0.0, double rMax = 1000.0, int decimals = 1)
        : rMin(rMin), rMax(rMax), decimals(decimals) {}

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

    void paint(juce::Graphics& g) override {
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::bgLight));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
        g.setFont(juce::Font(juce::FontOptions(14.0f)));
        g.drawFittedText(juce::String(value, decimals), getLocalBounds().reduced(4),
                         juce::Justification::centred, 1);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        startDragging = false;
        dragStartY = e.getPosition().y;
        dragStartValue = value;
    }

    void mouseDrag(const juce::MouseEvent& e) override {
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
        if (!startDragging)
            showTextEntryDialog();
        startDragging = false;
    }

    void mouseDoubleClick(const juce::MouseEvent&) override {
        showTextEntryDialog();
    }

    void showTextEntryDialog() {
        class EditorDialog : public juce::Component {
        public:
            juce::DialogWindow* parentWindow = nullptr;
            
            EditorDialog(double initialVal, double minVal, double maxVal, int dec,
                        std::function<void(double)> onOk, std::function<void()> onCancel)
                : minVal(minVal), maxVal(maxVal), decimals(dec), onOk(onOk), onCancel(onCancel)
            {
                addAndMakeVisible(textEditor);
                textEditor.setText(juce::String(initialVal, decimals), false);
                textEditor.setSelectAllWhenFocused(true);
                textEditor.setInputRestrictions(8, "0123456789.");
                
                addAndMakeVisible(okButton);
                okButton.setButtonText("OK");
                okButton.onClick = [this, minVal, maxVal, onOk] {
                    double newVal = textEditor.getText().getDoubleValue();
                    newVal = juce::jlimit(minVal, maxVal, newVal);
                    if (onOk) onOk(newVal);
                    if (parentWindow) parentWindow->closeButtonPressed();
                };
                
                addAndMakeVisible(cancelButton);
                cancelButton.setButtonText("Cancel");
                cancelButton.onClick = [this, onCancel] {
                    if (onCancel) onCancel();
                    if (parentWindow) parentWindow->closeButtonPressed();
                };
                
                setSize(200, 80);
                textEditor.grabKeyboardFocus();
            }
            
            bool keyPressed(const juce::KeyPress& key) override {
                if (key == juce::KeyPress::returnKey) {
                    okButton.triggerClick();
                    return true;
                }
                return false;
            }
            
            void resized() override {
                textEditor.setBounds(10, 10, 180, 24);
                okButton.setBounds(40, 45, 50, 24);
                cancelButton.setBounds(100, 45, 60, 24);
            }
            
        private:
            juce::TextEditor textEditor;
            juce::TextButton okButton, cancelButton;
            double minVal, maxVal;
            int decimals;
            std::function<void(double)> onOk;
            std::function<void()> onCancel;
        };
        
        auto* dialog = new EditorDialog(value, rMin, rMax, decimals,
            [this](double v) { setValue(v, juce::sendNotification); },
            []() {});
        
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned(dialog);
        o.dialogTitle = "Edit Value";
        o.dialogBackgroundColour = juce::Colour(LookAndFeel_BlockShuffler::bgMedium);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        auto* win = o.launchAsync();
        dialog->parentWindow = win;
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
    DraggableNumberBox tempoField { 20.0, 300.0, 1 };
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
    juce::OwnedArray<juce::Label> stackBlockLabels; ///< one label per block in group
    juce::OwnedArray<juce::Slider> stackBlockProbSliders; ///< one probability slider per block
    int lastBuiltStackGroup      = -2;            ///< detect group changes for block list

    // Track Y+H of the stack section for paint() tint
    int stackSectionY = -1;
    int stackSectionH = 0;

    void rebuildStackCountRows();
    void rebuildStackBlockLabels();
    void rebuildStackBlockProbRows();

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
