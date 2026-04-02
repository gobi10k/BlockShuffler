#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "MainComponent.h"

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
                             juce::Colour(0xFF1E1E1E),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);

            // Set up audio device to drive the playback engine.
            // Request the project's default sample rate (48 kHz) so that the device
            // and clip buffers run at the same rate and no pitch shift occurs during
            // playback.  If the hardware doesn't support 48 kHz, JUCE will choose
            // the nearest available rate; the diagnostic logging will show the
            // actual rate chosen vs. project.sampleRate.
            deviceManager.initialiseWithDefaultDevices(0, 2);
            {
                juce::AudioDeviceManager::AudioDeviceSetup setup;
                deviceManager.getAudioDeviceSetup(setup);
                setup.sampleRate = 48000.0;
                deviceManager.setAudioDeviceSetup(setup, true);
            }
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
