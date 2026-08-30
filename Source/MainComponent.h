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
#include "UI/SplitLayout.h"

namespace BlockShuffler {

class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer,
                      public juce::FileDragAndDropTarget,
                      public juce::ChangeListener {
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

    // Keyboard handling via Component::keyPressed
    bool keyPressed(const juce::KeyPress& key) override;

    // Called by editor timer to refresh transport display
    void updateTimeDisplay();

    /** Load a project from a .bsp file. Public so Main.cpp can route Finder/command-line opens here. */
    void loadProject(const juce::File& file);

    /** SPLITTER: called by the resizer bar after each drag step. Reads the dragged
     *  position back out of the layout manager, clamps it through SplitLayout, and
     *  relays out. Public only so the bar subclass below can reach it. */
    void splitterMoved();

private:
    PlaybackEngine&    engine;
    ArrangementResolver resolver;
    juce::Random       rng;

    LookAndFeel_BlockShuffler customLookAndFeel;
    juce::TooltipWindow       tooltipWindow { this };
    std::unique_ptr<Project>  project;
    juce::File                currentProjectFile;

    Block*       selectedBlock   = nullptr;
    juce::String selectedBlockId;   // survive undo-rebuild
    Block*       lastPlayingBlock = nullptr;  // tracks which block the waveform is following during playback

    ResolvedArrangement currentArrangement;  // kept so updateTimeDisplay can find playing block

    ClipWaveformView waveformView;
    BlockStrip       blockStrip;
    BlockLinkOverlay linkOverlay;
    juce::Viewport  inspectorViewport;
    InspectorPanel   inspectorPanel;
    TransportBar     transportBar;

    // ── SPLITTER (2026-08-21): draggable divider, waveform above / blocks below ──
    // splitLayout is the DRAG TRANSPORT only: StretchableLayoutResizerBar needs a
    // StretchableLayoutManager to push positions into, and the manager gives us the
    // native up/down resize cursor and smooth tracking for free. The authority on
    // the bounds actually applied is SplitLayout::clampBlocksHeight() in resized(),
    // because the manager overflows (rather than clamps) when space runs short.
    struct SplitterBar : juce::StretchableLayoutResizerBar {
        SplitterBar(juce::StretchableLayoutManager* lm, MainComponent& o)
            : juce::StretchableLayoutResizerBar(lm, 1, false), owner(o) {}
        void hasBeenMoved() override { owner.splitterMoved(); }
        // Persist once per gesture, not once per drag pixel.
        void mouseUp(const juce::MouseEvent&) override { owner.saveSplitterPosition(); }
        MainComponent& owner;
    };

    juce::StretchableLayoutManager splitLayout;
    SplitterBar                    splitterBar { &splitLayout, *this };

    /// The user's INTENT: the strip height they last dragged to (or restored).
    /// Kept separate from what is applied so that shrinking the window and growing
    /// it again restores the chosen split instead of sticking at the clamped value.
    int desiredBlocksHeight = SplitLayout::blocksDefaultH;
    /// Currently APPLIED strip height. Always the output of clampBlocksHeight.
    int blocksPaneHeight = SplitLayout::blocksDefaultH;
    /// Height of the waveform+bar+blocks area, cached by resized() for splitterMoved().
    int splitAreaHeight  = 0;

    /// Per-user divider persistence. NOT in the .bsp — the project format is untouched.
    juce::ApplicationProperties appProps;
    void loadSplitterPosition();

public:
    /** SPLITTER: writes the current divider to per-user app properties. Public so
     *  the bar subclass can call it on mouseUp (once per gesture, not per pixel). */
    void saveSplitterPosition();

private:

    static constexpr int inspectorWidth   = 210;
    static constexpr int transportHeight  = 56;

    void onPlayPressed();
    void onStopPressed();
    void onRewindPressed();
    void saveProject();
    void saveProjectAs();
    void openProject();
    void exportProject();
    void applyBlockSelection(Block* block);  // updates all views consistently
    void playBlock(const juce::String& blockId);
    void playClip(const juce::String& clipId);
    void updateWindowTitle(const juce::String& projectName);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace BlockShuffler
