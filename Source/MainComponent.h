#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Model/Project.h"
#include "Audio/PlaybackEngine.h"
#include "Audio/ArrangementResolver.h"
#include "Audio/ExportRenderer.h"
#include "UI/BlockStrip.h"
#include "UI/ClipWaveformView.h"
#include "UI/ClipListPanel.h"
#include "UI/InspectorPanel.h"
#include "UI/TransportBar.h"
#include "UI/BlockLinkOverlay.h"
#include "UI/LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer,
                      public juce::FileDragAndDropTarget,
                      public juce::ChangeListener,
                      public juce::KeyListener {
public:
    explicit MainComponent(PlaybackEngine& engine);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // ChangeListener
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // KeyListener
    bool keyPressed(const juce::KeyPress& key, juce::Component* originator) override;

    // Called by editor timer to refresh transport display
    void updateTimeDisplay();

private:
    PlaybackEngine&    engine;
    ArrangementResolver resolver;
    juce::Random       rng;

    LookAndFeel_BlockShuffler customLookAndFeel;
    juce::TooltipWindow       tooltipWindow { this, 600 };  // 600 ms hover delay
    std::unique_ptr<Project>  project;
    juce::File                currentProjectFile;

    Block*       selectedBlock   = nullptr;
    juce::String selectedBlockId;  // survive undo-rebuild

    ResolvedArrangement currentArrangement;  // kept so updateTimeDisplay can find playing block

    ClipWaveformView waveformView;
    BlockStrip       blockStrip;
    BlockLinkOverlay linkOverlay;
    InspectorPanel   inspectorPanel;
    TransportBar     transportBar;

    static constexpr int inspectorWidth   = 210;
    static constexpr int transportHeight  = 48;
    static constexpr int blockStripHeight = 160;

    void onPlayPressed();
    void onStopPressed();
    void onRewindPressed();
    void saveProject();
    void saveProjectAs();
    void openProject();
    void loadProject(const juce::File& file);
    void exportProject();
    void applyBlockSelection(Block* block);  // updates all views consistently

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace BlockShuffler
