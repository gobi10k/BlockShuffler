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
    using LF = LookAndFeel_BlockShuffler;

    // ── Background: pure black ─────────────────────────────────────────────
    g.fillAll(juce::Colour(LF::bgDark));

    // Hairline separator at top — very dark panel colour
    g.setColour(juce::Colour(LF::panelCol));
    g.drawLine(0.0f, 0.0f, (float)getWidth(), 0.0f, 1.0f);

    // ── Time display (centre-left, secondary text) ─────────────────────────
    auto timeText = formatTime(currentSecs) + " / " + formatTime(totalSecs);
    g.setColour(juce::Colour(LF::textSecondary));
    g.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Regular")));
    g.drawText(timeText, timeDisplayArea, juce::Justification::centred);

    // ── RiverMix. wordmark (centre-right) ─────────────────────────────────
    // Matches the logo: white "River" + cyan→blue gradient "Mix."
    if (brandingArea.isEmpty()) return;

    const float fy      = (float)brandingArea.getCentreY();
    const float fontSz  = 16.0f;
    const float totalW  = 108.0f;  // approximate rendered width
    const float startX  = brandingArea.getX()
                          + (brandingArea.getWidth() - totalW) / 2.0f;

    g.setFont(juce::Font(juce::FontOptions(fontSz).withStyle("Bold")));

    // "River" — pure white
    g.setColour(juce::Colour(LF::textPrimary));
    g.drawText("River",
               juce::Rectangle<float>(startX, fy - fontSz / 2.0f - 1, 58, fontSz + 2),
               juce::Justification::centredLeft, false);

    // "Mix." — electric cyan (matches the logo's gradient highlight end)
    g.setColour(juce::Colour(LF::accentCyan));
    g.drawText("Mix.",
               juce::Rectangle<float>(startX + 58, fy - fontSz / 2.0f - 1, 52, fontSz + 2),
               juce::Justification::centredLeft, false);
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

    // Center: split — time display on left half, branding on right half
    brandingArea    = area.removeFromRight(area.getWidth() / 2);
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
