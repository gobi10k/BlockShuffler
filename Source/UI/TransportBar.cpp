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

    // Mark Play as a primary button so drawButtonBackground applies the accent treatment
    playBtn.getProperties().set("primary", true);

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
    // ── Background ────────────────────────────────────────────────────────────
    g.fillAll(juce::Colour(LookAndFeel_BlockShuffler::bgDark));

    // Top separator line
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::borderSubtle));
    g.drawLine(0.0f, 0.0f, (float)getWidth(), 0.0f, 1.0f);

    // ── Time display — current in primary, separator + total in tertiary ─────
    {
        auto font     = LookAndFeel_BlockShuffler::monoFont(14.0f);
        auto curText  = formatTime(currentSecs);
        auto sepText  = juce::String(" / ");
        auto totText  = formatTime(totalSecs);
        float w1      = font.getStringWidthFloat(curText);
        float w2      = font.getStringWidthFloat(sepText + totText);
        float totalW  = w1 + w2;
        float startX  = (float)timeDisplayArea.getCentreX() - totalW * 0.5f;
        float midY    = (float)timeDisplayArea.getCentreY();
        const float rowH = 18.0f;
        g.setFont(font);
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textPrimary));
        g.drawText(curText, juce::Rectangle<float>(startX, midY - rowH * 0.5f, w1 + 1.0f, rowH),
                   juce::Justification::centredLeft, false);
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::textTertiary));
        g.drawText(sepText + totText, juce::Rectangle<float>(startX + w1, midY - rowH * 0.5f, w2 + 1.0f, rowH),
                   juce::Justification::centredLeft, false);
    }

    // ── App branding ──────────────────────────────────────────────────────────
    if (brandingArea.isEmpty()) return;

    const int iconW = 28;
    const int iconH = 18;
    const int gap   = 6;
    const int cy    = brandingArea.getCentreY();

    // Mini block-arrangement icon (3 coloured rectangles + arrows)
    const int iconX = brandingArea.getX() + (brandingArea.getWidth() - iconW - gap - 80) / 2;
    const int iconY = cy - iconH / 2;

    float r = 2.5f;
    // Block 1 — Neon Cyan
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol));
    g.fillRoundedRectangle((float)iconX, (float)iconY, 7.0f, (float)iconH, r);
    // Block 2 — Electric Violet (slightly shorter, offset down)
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentViolet));
    g.fillRoundedRectangle((float)(iconX + 10), (float)(iconY + 3), 7.0f, (float)(iconH - 6), r);
    // Block 3 — Neon Cyan
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol));
    g.fillRoundedRectangle((float)(iconX + 20), (float)iconY, 7.0f, (float)iconH, r);

    // Connector dots — accent at reduced alpha for consistency
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol).withAlpha(0.55f));
    g.fillEllipse((float)(iconX + 8), (float)(cy - 1), 2.0f, 2.0f);
    g.fillEllipse((float)(iconX + 18), (float)(cy - 1), 2.0f, 2.0f);

    // App name
    const int textX = iconX + iconW + gap;
    g.setColour(juce::Colour(LookAndFeel_BlockShuffler::accentCol));
    g.setFont(LookAndFeel_BlockShuffler::uiFontBold(14.0f));
    g.drawText("BlockShuffler",
               juce::Rectangle<int>(textX, cy - 9, 90, 18),
               juce::Justification::centredLeft);
}

void TransportBar::resized() {
    auto area = getLocalBounds().reduced(8, 6);
    const int gap = 4;
    const int btnH = 28;

    // Left: transport controls
    rewindBtn.setBounds(area.removeFromLeft(36).withSizeKeepingCentre(36, btnH));
    area.removeFromLeft(gap);
    playBtn  .setBounds(area.removeFromLeft(68).withSizeKeepingCentre(68, btnH));
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
