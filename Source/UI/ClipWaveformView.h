#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Model/Block.h"
#include "LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

/**
 * A single clip row: colored header with editable name + waveform with markers.
 */
class ClipRowComponent : public juce::Component,
                         public juce::SettableTooltipClient {
public:
    ClipRowComponent(Clip& clip,
                     double projectSampleRate,
                     std::function<void()> onSelected,
                     std::function<void()> onRepaintNeeded,
                     std::function<void()> onRemoveRequested);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    void setSelected(bool sel);
    Clip* getClip() { return clip.get(); }
    juce::Rectangle<int> waveArea() const { return getLocalBounds().withTrimmedTop(headerH).reduced(2, 2); }

    static constexpr int headerH   = 24;
    static constexpr int markerHit = 7;

private:
    juce::WeakReference<Clip> clip;
    double projectSampleRate = 48000.0;
    bool   selected = false;

    std::function<void()> onSelectedCallback;
    std::function<void()> onRepaintCallback;
    std::function<void()> onRemoveCallback;

public:
    // Wired by ClipWaveformView so context-menu and marker-drag mutations are undoable
    std::function<juce::var()>            onCaptureSnapshot;
    std::function<void(const juce::var&)> onUndoableMutation;

    /// Called when "Play Clip" is chosen from the context menu.
    std::function<void(const juce::String&)> onPlayClipRequested;

    // Used to compute effective (normalized) probability across all sibling clips
    Block* ownerBlock = nullptr;

private:
    juce::Label nameLabel;
    juce::var   nameLabelEditPre;   // snapshot captured when name editor opens
    juce::var   markerDragPre;      // snapshot captured when a marker drag begins

    enum class DragTarget { None, StartMarker, EndMarker, Clip };
    DragTarget activeDrag = DragTarget::None;
    int clipDragStartX = 0;
    int clipDragStartY = 0;

    int     sampleToX(int64_t sample) const;
    int64_t xToSample(int x) const;
    void    renderWaveform(juce::Graphics& g, juce::Rectangle<int> area) const;
    void    showContextMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipRowComponent)
};

/** Viewport subclass that routes Cmd+scroll to a zoom callback instead of scrolling. */
class ZoomableViewport : public juce::Viewport {
public:
    std::function<void(float deltaY)> onZoomScroll;

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(LookAndFeel_BlockShuffler::bgDark));
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override {
        if (e.mods.isCommandDown() && onZoomScroll) {
            onZoomScroll(w.deltaY);
        } else {
            juce::Viewport::mouseWheelMove(e, w);
        }
    }
};

/**
 * Scrollable view of all clips in the selected Block.
 * Has an "Add Clip" button that opens a FileChooser.
 */
class ClipWaveformView : public juce::Component,
                         public juce::ChangeListener {
public:
    ClipWaveformView();
    ~ClipWaveformView() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

    void setBlock(Block* block, double sampleRate, juce::AudioFormatManager* fmtMgr = nullptr);
    Clip*  getSelectedClip()  const { return selectedClip; }
    Block* getCurrentBlock()  const { return currentBlock; }

    /** Set the currently playing clip and its position.
        @param clipId The ID of the clip being played (empty = stop playback)
        @param samplePos Position within the clip's audio buffer (0 = start)
        @param sampleRate The sample rate for samplePos calculation */
    void setPlayingClip(const juce::String& clipId, int64_t samplePos, double sampleRate);

    double defaultTempo = 120.0;  ///< Applied to new clips when loaded; sync'd from Project::defaultClipTempo

    std::function<void(Clip*)> onClipSelected;

    /// Called when "Play Clip" is chosen from a clip's context menu.
    std::function<void(const juce::String&)> onPlayClipRequested;

    // Wired by MainComponent so clip mutations are undoable
    std::function<juce::var()>            onCaptureSnapshot;
    std::function<void(const juce::var&)> onUndoableMutation;

    // juce::ChangeListener
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void paintOverChildren(juce::Graphics& g) override;

private:
    juce::WeakReference<Block> currentBlock;
    Clip*  selectedClip  = nullptr;
    juce::AudioFormatManager* formatManager = nullptr;
    double projectSampleRate = 48000.0;

    ZoomableViewport viewport;
    struct ContentArea : public juce::Component {
        ContentArea() { setOpaque(true); }
        void paint(juce::Graphics& g) override {
            g.fillAll(juce::Colour(LookAndFeel_BlockShuffler::bgDark));
        }
    };
    ContentArea contentArea;
    juce::OwnedArray<ClipRowComponent> clipRows;
    juce::TextButton addClipBtn  { "+ Add Clip" };
    juce::TextButton zoomInBtn   { "+" };
    juce::TextButton zoomOutBtn  { "-" };
    juce::TextButton zoomFitBtn  { "Fit" };

    static constexpr int zoomBarH = 24;

    juce::String playingClipId;
    int64_t      playingSamplePos = 0;
    double       playingSampleRate = 48000.0;
    float zoomFactor = 1.0f;         // 1.0 = fit; >1 = zoomed in

    static constexpr int rowH   = 110;
    static constexpr int rowGap = 2;
    static constexpr int btnH   = 30;

    void rebuildRows();
    void selectClip(Clip* clip);
    void removeClip(Clip* clip);
    void browseForClip();
    float computeMaxZoom() const;
    /** Change zoom keeping the time under the view CENTRE fixed (13.4 UX:
     *  without anchoring, the kept pixel-x drifts the view to the clip start
     *  on every zoom step). Captures the centre time-fraction BEFORE the zoom
     *  changes, restores it after the deferred re-layout, clamped. */
    void setZoomAnchored(float newFactor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipWaveformView)
};

} // namespace BlockShuffler
