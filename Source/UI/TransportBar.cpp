#include "TransportBar.h"
#include "LookAndFeel_BlockShuffler.h"

#if __has_include(<BinaryData.h>)
  #include <BinaryData.h>
  #define TRANSPORT_HAS_BINARY 1
#else
  #define TRANSPORT_HAS_BINARY 0
#endif

namespace BlockShuffler {

TransportBar::TransportBar() {
    rewindBtn.onClick = [this] { if (onRewind) onRewind(); };
    playBtn  .onClick = [this] { if (onPlay)   onPlay();   };
    stopBtn  .onClick = [this] { if (onStop)   onStop();   };
    exportBtn.onClick = [this] { if (onExport) onExport(); };

#if TRANSPORT_HAS_BINARY
    logoImage = juce::ImageCache::getFromMemory(BinaryData::icon_png,
                                                BinaryData::icon_pngSize);
#endif
    saveBtn  .onClick = [this] { if (onSave)   onSave();   };
    saveAsBtn.onClick = [this] { if (onSaveAs) onSaveAs(); };
    openBtn  .onClick = [this] { if (onOpen)   onOpen();   };

    // Mark Play as a primary button so drawButtonBackground applies the accent treatment
    playBtn.getProperties().set("primary", true);

    rewindBtn.setTooltip("Rewind to start");
    playBtn  .setTooltip("Play / Pause  [Space]");
    stopBtn  .setTooltip("Stop and rewind");
    exportBtn.setTooltip("Export arrangement to WAV, FLAC, or BSF bundle");
    saveBtn  .setTooltip("Save project  [Cmd+S]");
    saveAsBtn.setTooltip("Save project to a new location  [Cmd+Shift+S]");
    openBtn  .setTooltip("Open project  [Cmd+O]");

    addAndMakeVisible(rewindBtn);
    addAndMakeVisible(playBtn);
    addAndMakeVisible(stopBtn);
    addAndMakeVisible(exportBtn);
    addAndMakeVisible(saveBtn);
    addAndMakeVisible(saveAsBtn);
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
        float w1      = LookAndFeel_BlockShuffler::measureTextWidth(font, curText);
        float w2      = LookAndFeel_BlockShuffler::measureTextWidth(font, sepText + totText);
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

    // ── Company logo ──────────────────────────────────────────────────────────
    if (brandingArea.getWidth() < 20 || brandingArea.getHeight() < 4) return;

    if (logoImage.isValid()) {
        // Scale to fill the full branding height, preserve aspect ratio, centre horizontally
        const int maxH  = brandingArea.getHeight();
        const int maxW  = brandingArea.getWidth();
        float scale     = juce::jmin((float)maxH / (float)logoImage.getHeight(),
                                     (float)maxW / (float)logoImage.getWidth());
        int drawW       = juce::roundToInt((float)logoImage.getWidth()  * scale);
        int drawH       = juce::roundToInt((float)logoImage.getHeight() * scale);
        int drawX       = brandingArea.getCentreX() - drawW / 2;
        int drawY       = brandingArea.getCentreY() - drawH / 2;
        g.drawImage(logoImage, drawX, drawY, drawW, drawH, 0, 0,
                    logoImage.getWidth(), logoImage.getHeight());
    }
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
    openBtn  .setBounds(area.removeFromRight(60).withSizeKeepingCentre(60, btnH));
    area.removeFromRight(gap);
    saveBtn  .setBounds(area.removeFromRight(60).withSizeKeepingCentre(60, btnH));
    area.removeFromRight(gap);
    saveAsBtn.setBounds(area.removeFromRight(68).withSizeKeepingCentre(68, btnH));
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
