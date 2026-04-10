#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "MainComponent.h"
#include "UI/LookAndFeel_BlockShuffler.h"

/** Thin AudioSource adapter that drives a PlaybackEngine from the audio device callback. */
class PlaybackEngineSource final : public juce::AudioSource {
public:
    explicit PlaybackEngineSource(BlockShuffler::PlaybackEngine& e) : engine(e) {}

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
        engine.prepareToPlay(sampleRate, samplesPerBlockExpected);
        mixBuffer.setSize(2, samplesPerBlockExpected, false, true, true);
    }

    void releaseResources() override {
        engine.releaseResources();
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override {
        info.clearActiveBufferRegion();
        if (mixBuffer.getNumSamples() < info.numSamples)
            mixBuffer.setSize(2, info.numSamples, false, true, false);

        mixBuffer.clear();
        engine.getNextAudioBlock(mixBuffer, info.numSamples);

        const int nCh = juce::jmin(info.buffer->getNumChannels(), mixBuffer.getNumChannels());
        for (int ch = 0; ch < nCh; ++ch)
            info.buffer->addFrom(ch, info.startSample, mixBuffer, ch, 0, info.numSamples);
    }

private:
    BlockShuffler::PlaybackEngine& engine;
    juce::AudioBuffer<float>       mixBuffer;
};

/**
 * Draw the RiverMix app icon programmatically into a juce::Image.
 * Approximates the flowing logo mark: white outer arcs, blue→cyan
 * inner strokes, teardrop, on pure black.
 */
static juce::Image createAppIcon()
{
    const int sz = 256;
    juce::Image img(juce::Image::ARGB, sz, sz, true);
    juce::Graphics g(img);

    // Brand colors
    const juce::Colour black  { 0xFF000000 };
    const juce::Colour white  { 0xFFFFFFFF };
    const juce::Colour cyan   { 0xFF00C8FF };
    const juce::Colour mid    { 0xFF2B7FFF };
    const juce::Colour deep   { 0xFF1E40FF };

    const float cx = sz / 2.0f;
    const float cy = sz / 2.0f;

    // Pure black background
    g.fillAll(black);

    // ── Outer white circle arcs (left 110°–250°, right 290°–70°) ─────────
    g.setColour(white.withAlpha(0.9f));
    juce::Path leftArc, rightArc;
    leftArc .addCentredArc(cx, cy, 90, 90, 0,
                            juce::degreesToRadians(110.0f),
                            juce::degreesToRadians(250.0f), true);
    rightArc.addCentredArc(cx, cy, 90, 90, 0,
                            juce::degreesToRadians(290.0f),
                            juce::degreesToRadians(430.0f), true);
    g.strokePath(leftArc,  juce::PathStrokeType(7.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    g.strokePath(rightArc, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    // ── Inner arcs (semi-transparent) ────────────────────────────────────
    g.setColour(white.withAlpha(0.35f));
    juce::Path lIn, rIn;
    lIn.addCentredArc(cx, cy, 70, 70, 0,
                       juce::degreesToRadians(118.0f), juce::degreesToRadians(242.0f), true);
    rIn.addCentredArc(cx, cy, 70, 70, 0,
                       juce::degreesToRadians(298.0f), juce::degreesToRadians(422.0f), true);
    g.strokePath(lIn, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    g.strokePath(rIn, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    // ── Top left wing curve (cyan) ────────────────────────────────────────
    g.setColour(cyan);
    juce::Path wingL;
    wingL.startNewSubPath(cx, 28.0f);
    wingL.cubicTo(cx - 24, 24, cx - 58, 20, cx - 82, 38);
    g.strokePath(wingL, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    // ── Top right wing curve (deep blue) ─────────────────────────────────
    g.setColour(deep);
    juce::Path wingR;
    wingR.startNewSubPath(cx, 28.0f);
    wingR.cubicTo(cx + 24, 24, cx + 58, 20, cx + 82, 38);
    g.strokePath(wingR, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    // ── Vertical stem ─────────────────────────────────────────────────────
    g.setColour(cyan);
    g.drawLine(cx, 28, cx, 88, 7.0f);

    // ── Fork (left prong — cyan) ──────────────────────────────────────────
    g.setColour(cyan);
    juce::Path forkL;
    forkL.startNewSubPath(cx, 88);
    forkL.cubicTo(cx - 12, 104, cx - 22, 126, cx, 140);
    g.strokePath(forkL, juce::PathStrokeType(6.5f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    // ── Fork (right prong — deep blue) ───────────────────────────────────
    g.setColour(deep);
    juce::Path forkR;
    forkR.startNewSubPath(cx, 88);
    forkR.cubicTo(cx + 12, 104, cx + 22, 126, cx, 140);
    g.strokePath(forkR, juce::PathStrokeType(6.5f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    // ── Teardrop (mid blue fill, cyan tint) ───────────────────────────────
    g.setColour(mid);
    juce::Path drop;
    drop.addEllipse(cx - 16, 140, 32, 40);
    drop.startNewSubPath(cx - 10, 142);
    drop.quadraticTo(cx, 128, cx + 10, 142);
    g.fillPath(drop);
    g.setColour(cyan.withAlpha(0.55f));
    g.fillPath(drop);

    // ── Lower stem ────────────────────────────────────────────────────────
    g.setColour(deep);
    g.drawLine(cx, 182, cx, 220, 7.0f);

    // ── Cap dashes at the top ─────────────────────────────────────────────
    g.setColour(white.withAlpha(0.75f));
    g.drawLine(cx - 14, 18, cx + 14, 18, 5.5f);
    g.setColour(white.withAlpha(0.45f));
    g.drawLine(cx - 9,  10, cx + 9,  10, 4.0f);

    return img;
}

class BlockShufflerApplication : public juce::JUCEApplication {
public:
    BlockShufflerApplication() = default;

    const juce::String getApplicationName() override    { return "BlockShuffler"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override           { return true; }

    void initialise(const juce::String& /*commandLine*/) override {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override {
        quit();
    }

    class MainWindow : public juce::DocumentWindow,
                       private juce::Timer {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name,
                             juce::Colour(BlockShuffler::LookAndFeel_BlockShuffler::bgDark),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);

            // Set the window icon to the brand icon (programmatic, no file I/O needed)
            setIcon(createAppIcon());
            juce::Process::setDockIconVisible(true);

            // Set up audio device to drive the playback engine
            deviceManager.initialiseWithDefaultDevices(0, 2);
            deviceManager.addAudioCallback(&audioSourcePlayer);
            audioSourcePlayer.setSource(&engineSource);

            mainComponent = std::make_unique<BlockShuffler::MainComponent>(engine);
            setContentNonOwned(mainComponent.get(), false);
            // false = no JUCE corner-dragger widget; macOS native title bar handles resizing
            setResizable(true, false);
            centreWithSize(1200, 700);
            setVisible(true);  // ← native peer is created HERE on macOS

            // Apply size limits AFTER setVisible so the native peer already exists
            // when we set the constrainer. setResizeLimits before setVisible is ignored
            // by the macOS NSWindow delegate because the peer hasn't been created yet.
            sizeConstrainer.setMinimumSize(800, 600);
            sizeConstrainer.setMaximumSize(2560, 1600);
            setConstrainer(&sizeConstrainer);

            startTimerHz(30);  // 30 fps transport display refresh
        }

        ~MainWindow() override {
            stopTimer();
            // Clear MainComponent BEFORE audio callback stops to ensure
            // PlaybackEngine is no longer being actively driven by it
            mainComponent = nullptr;
            audioSourcePlayer.setSource(nullptr);
            deviceManager.removeAudioCallback(&audioSourcePlayer);
            deviceManager.closeAudioDevice();
        }

        void timerCallback() override {
            if (mainComponent) mainComponent->updateTimeDisplay();
        }

        void resized() override {
            DocumentWindow::resized();
            // Enforce minimum size. The sizeConstrainer should prevent this,
            // but the native-title-bar path on macOS can bypass it. This
            // snap-back is the guaranteed fallback: if the window is already
            // within bounds the condition is false and there is no recursion.
            const int w = juce::jmax(800, getWidth());
            const int h = juce::jmax(600, getHeight());
            if (w != getWidth() || h != getHeight())
                setSize(w, h);
        }

        void closeButtonPressed() override {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        BlockShuffler::PlaybackEngine          engine;
        PlaybackEngineSource                   engineSource { engine };
        juce::AudioDeviceManager               deviceManager;
        juce::AudioSourcePlayer                audioSourcePlayer;
        std::unique_ptr<BlockShuffler::MainComponent> mainComponent;
        juce::ComponentBoundsConstrainer       sizeConstrainer;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(BlockShufflerApplication)
