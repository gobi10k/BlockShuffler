#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include "../Utils/UuidGenerator.h"

namespace BlockShuffler {

class Clip : public juce::ChangeBroadcaster {
public:
    Clip()
        : id(generateUuid()),
          name("Untitled Clip"),
          color(juce::Colours::white),
          startMark(0),
          endMark(0),
          probability(1.0f),
          tempo(120.0),
          retainLeadInTempo(false),
          retainTailTempo(false),
          gridOffsetSamples(0),
          isSongEnder(false),
          isDone(false) {}

    // Identity
    juce::String id;
    juce::String name;
    juce::Colour color;

    // Audio
    juce::File audioFile;
    juce::AudioBuffer<float> audioBuffer;
    double nativeSampleRate = 0.0;

    // Markers (in samples, relative to audioBuffer start)
    int64_t startMark;
    int64_t endMark;

    // Randomization
    float probability;  // 0.0 – 1.0

    // Tempo / Grid
    double tempo;
    bool retainLeadInTempo;
    bool retainTailTempo;
    int64_t gridOffsetSamples;

    // Flags
    bool isSongEnder;
    bool isDone;

    // Computed
    int64_t getLeadInLength() const { return startMark; }
    int64_t getBodyLength() const { return endMark - startMark; }
    int64_t getTailLength() const { return audioBuffer.getNumSamples() - endMark; }
    int64_t getTotalLength() const { return audioBuffer.getNumSamples(); }

    /** Load audio from file. Returns true on success.
     *  If targetSampleRate > 0 and differs from the file's native rate, the
     *  buffer is resampled to targetSampleRate using LagrangeInterpolator. */
    bool loadFromFile(const juce::File& file, juce::AudioFormatManager& formatManager,
                      double targetSampleRate = 0.0) {
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (reader == nullptr) return false;

        audioBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
        reader->read(&audioBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
        nativeSampleRate = reader->sampleRate;
        audioFile = file;
        startMark = 0;
        endMark = reader->lengthInSamples;
        if (name == "Untitled Clip")
            name = file.getFileNameWithoutExtension();

        // Resample to project rate if needed
        if (targetSampleRate > 0.0 && nativeSampleRate > 0.0
            && std::abs(nativeSampleRate - targetSampleRate) > 0.5)
        {
            // LagrangeInterpolator: ratio = inputRate / outputRate.
            // ratio > 1 → fewer output samples (input rate higher than target, e.g. 96k -> 48k).
            // ratio < 1 → more output samples (input rate lower than target, e.g. 44.1k -> 48k).
            double ratio  = nativeSampleRate / targetSampleRate;
            int    outLen = juce::jmax(1, (int)((double)reader->lengthInSamples / ratio + 0.5));
            int    numCh  = audioBuffer.getNumChannels();

            juce::AudioBuffer<float> resampled(numCh, outLen);
            resampled.clear();

            for (int ch = 0; ch < numCh; ++ch)
            {
                juce::LagrangeInterpolator interp;
                interp.reset();
                interp.process(ratio,
                               audioBuffer.getReadPointer(ch),
                               resampled.getWritePointer(ch),
                               outLen);
            }

            audioBuffer = std::move(resampled);
            endMark     = (int64_t)outLen;
        }

        return true;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Clip)
};

} // namespace BlockShuffler
