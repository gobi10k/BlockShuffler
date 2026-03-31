#include "PluginProcessor.h"
#include "MainComponent.h"

namespace BlockShuffler {

//==============================================================================
// Editor — hosts MainComponent, wires transport to PlaybackEngine
//==============================================================================
class BlockShufflerEditor : public juce::AudioProcessorEditor,
                            private juce::Timer {
public:
    explicit BlockShufflerEditor(BlockShufflerProcessor& p)
        : AudioProcessorEditor(p),
          processor(p),
          mainComponent(p.getPlaybackEngine())
    {
        addAndMakeVisible(mainComponent);
        setSize(1200, 700);
        setResizable(true, true);
        startTimerHz(10);  // update time display 10×/sec
    }

    ~BlockShufflerEditor() override {
        stopTimer();
    }

    void resized() override {
        mainComponent.setBounds(getLocalBounds());
    }

private:
    void timerCallback() override {
        mainComponent.updateTimeDisplay();
    }

    BlockShufflerProcessor& processor;
    MainComponent mainComponent;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlockShufflerEditor)
};

//==============================================================================
// Processor
//==============================================================================
BlockShufflerProcessor::BlockShufflerProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{}

void BlockShufflerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    playbackEngine.prepareToPlay(sampleRate, samplesPerBlock);
}

void BlockShufflerProcessor::releaseResources() {
    playbackEngine.releaseResources();
}

void BlockShufflerProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;
    playbackEngine.getNextAudioBlock(buffer, buffer.getNumSamples());
}

juce::AudioProcessorEditor* BlockShufflerProcessor::createEditor() {
    return new BlockShufflerEditor(*this);
}

} // namespace BlockShuffler

//==============================================================================
// Plugin entry point
//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new BlockShuffler::BlockShufflerProcessor();
}
