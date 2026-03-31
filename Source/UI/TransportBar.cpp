#include "TransportBar.h"
#include "LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

TransportBar::TransportBar() {
    rewindBtn.onClick = [this] { if (onRewind) onRewind(); };
    playBtn  .onClick = [this] { if (onPlay)   onPlay();   };
    stopBtn  .onClick = [this] { if (onStop)   onStop();   };
    exportBtn.onClick = [this] { if (onExport) onExport(); };
    saveBtn  .onClick = [this] { if (onSave)   onSave();   };
    openBtn  .onClick = [this] { if (onOpen)   onOpen();   };

    rewindBtn.setTooltip("Rewind to start");
    playBtn  .setTooltip("Play / Pause  [Space]");
    stopBtn  .setTooltip("Stop and rewind");
    exportBtn.setTooltip("Export arrangement to WAV, FLAC, or BSF bundle");
    saveBtn  .setTooltip("Save project  [Cmd+S]");
    openBtn  .setTooltip("Open project  [Cmd+O]");

    addAndMakeVisible(rewindBtn);
    addAndMakeVisible(playBtn);
    addAndMakeVisible(stopBtn);
    addAndMakeVisible(exportBtn);
    addAndMakeVisible(saveBtn);
    addAndMakeVisible(openBtn);
}

void TransportBar::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(LookAndFeel_BlockShuffler::bgDark));
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::bgLight));
    g.drawLine(0.0f, 0.0f, (float)getWidth(), 0.0f, 1.0f);

    auto timeText = formatTime(currentSecs) + " / " + formatTime(totalSecs);
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
    g.setFont(14.0f);
    g.drawText(timeText, timeDisplayArea, juce::Justification::centred);
}

void TransportBar::resized() {
    auto area = getLocalBounds().reduced(8, 6);
    const int gap = 4;
    const int btnH = 28;

    // Left: transport controls
    rewindBtn.setBounds(area.removeFromLeft(36).withSizeKeepingCentre(36, btnH));
    area.removeFromLeft(gap);
    playBtn  .setBounds(area.removeFromLeft(60).withSizeKeepingCentre(60, btnH));
    area.removeFromLeft(gap);
    stopBtn  .setBounds(area.removeFromLeft(60).withSizeKeepingCentre(60, btnH));
    area.removeFromLeft(gap * 4);
    exportBtn.setBounds(area.removeFromLeft(72).withSizeKeepingCentre(72, btnH));
    area.removeFromLeft(gap * 2);

    // Right: file operations
    openBtn.setBounds(area.removeFromRight(60).withSizeKeepingCentre(60, btnH));
    area.removeFromRight(gap);
    saveBtn.setBounds(area.removeFromRight(60).withSizeKeepingCentre(60, btnH));
    area.removeFromRight(gap * 2);

    // Center: whatever remains is the time display zone
    timeDisplayArea = area;
}

void TransportBar::setIsPlaying(bool playing) {
    isPlaying = playing;
    playBtn.setButtonText(playing ? "Pause" : "Play");
    repaint();
}

void TransportBar::setTimeDisplay(double currentSec, double totalSec) {
    currentSecs = currentSec;
    totalSecs   = totalSec;
    repaint();
}

juce::String TransportBar::formatTime(double seconds) {
    int mins = (int)(seconds / 60.0);
    int secs = (int)(seconds) % 60;
    return juce::String::formatted("%02d:%02d", mins, secs);
}

} // namespace BlockShuffler
