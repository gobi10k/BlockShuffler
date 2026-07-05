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

/** Draw the BlockShuffler icon programmatically into a juce::Image. */
static juce::Image createAppIcon()
{
    using LF = BlockShuffler::LookAndFeel_BlockShuffler;
    const int sz = 256;
    juce::Image img(juce::Image::ARGB, sz, sz, true);
    juce::Graphics g(img);

    // Rounded background: Graphite Blue
    g.setColour(juce::Colour(LF::bgMedium));
    g.fillRoundedRectangle(0.0f, 0.0f, (float)sz, (float)sz, 48.0f);

    // Three blocks representing the arrangement strip
    const int bw = 46, bh = 90;
    const int by = (sz - bh) / 2;

    // Block 1 (left) — Neon Cyan
    g.setColour(juce::Colour(LF::accentCol));
    g.fillRoundedRectangle(28.0f, (float)by, (float)bw, (float)bh, 8.0f);
    g.setColour(juce::Colour(LF::bgMedium));
    g.fillRoundedRectangle(32.0f, (float)(by + 8), (float)(bw - 8), (float)(bh - 16), 5.0f);

    // Block 2 (centre) — Electric Violet, slightly shorter
    const int b2h = 58, b2y = (sz - b2h) / 2;
    g.setColour(juce::Colour(LF::accentViolet));
    g.fillRoundedRectangle(105.0f, (float)b2y, (float)bw, (float)b2h, 8.0f);
    g.setColour(juce::Colour(LF::bgMedium));
    g.fillRoundedRectangle(109.0f, (float)(b2y + 8), (float)(bw - 8), (float)(b2h - 16), 5.0f);

    // Block 3 (right) — Neon Cyan
    g.setColour(juce::Colour(LF::accentCol));
    g.fillRoundedRectangle(182.0f, (float)by, (float)bw, (float)bh, 8.0f);
    g.setColour(juce::Colour(LF::bgMedium));
    g.fillRoundedRectangle(186.0f, (float)(by + 8), (float)(bw - 8), (float)(bh - 16), 5.0f);

    // Connecting arrows — Hot Magenta
    g.setColour(juce::Colour(LF::accentMagenta));
    const float ay = (float)(sz / 2);
    g.drawLine(76.0f, ay, 103.0f, ay, 3.5f);
    g.drawLine(153.0f, ay, 180.0f, ay, 3.5f);

    // Tiny arrowheads
    juce::Path ah;
    ah.startNewSubPath(99.0f, ay - 6.0f);
    ah.lineTo(105.0f, ay);
    ah.lineTo(99.0f, ay + 6.0f);
    g.strokePath(ah, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
    juce::Path ah2;
    ah2.startNewSubPath(176.0f, ay - 6.0f);
    ah2.lineTo(182.0f, ay);
    ah2.lineTo(176.0f, ay + 6.0f);
    g.strokePath(ah2, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    return img;
}

class BlockShufflerApplication : public juce::JUCEApplication {
public:
    BlockShufflerApplication() = default;

    const juce::String getApplicationName() override    { return "BlockShuffler"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override           { return true; }

    void initialise(const juce::String& commandLine) override {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
        // Open a .bsp file passed on the command line (double-click from Finder/Explorer).
        if (commandLine.isNotEmpty()) {
            juce::String path = commandLine.trim();
            if (path.startsWithChar('"') && path.endsWithChar('"'))
                path = path.substring(1, path.length() - 1);
            juce::File f(path);
            if (f.existsAsFile() && f.hasFileExtension(".bsp"))
                mainWindow->openFile(f);
        }
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
#if JUCE_MAC
            juce::Process::setDockIconVisible(true);
#endif

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

        void openFile(const juce::File& file) {
            if (mainComponent)
                mainComponent->loadProject(file);
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
