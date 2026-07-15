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
    // The PNG has a pure-black background (RGB, no alpha channel).
    // Convert to ARGB and chroma-key it out so the logo blends seamlessly
    // against the transport bar's bgDark colour without a visible dark box.
    if (logoImage.isValid()) {
        juce::Image argb(juce::Image::ARGB, logoImage.getWidth(), logoImage.getHeight(), true);
        {
            juce::Graphics g2(argb);
            g2.drawImageAt(logoImage, 0, 0);
        }
        {
            juce::Image::BitmapData px(argb, juce::Image::BitmapData::readWrite);
            for (int y = 0; y < argb.getHeight(); ++y) {
                uint8_t* row = px.getLinePointer(y);
                for (int x = 0; x < argb.getWidth(); ++x) {
                    // JUCE ARGB memory layout per pixel: B, G, R, A
                    uint8_t* p = row + x * px.pixelStride;
                    uint8_t maxC = juce::jmax(p[0], juce::jmax(p[1], p[2]));  // max(B,G,R)
                    if (maxC < 20)
                        p[3] = 0;                                              // fully transparent
                    else if (maxC < 50)
                        p[3] = (uint8_t)(((int)maxC - 20) * 255 / 30);        // soft edge
                    // else leave alpha = 255 (opaque logo content)
                }
            }
        }
        logoImage = argb;
    }
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
    if (brandingArea.getWidth() >= 20 && logoImage.isValid()) {
        // Explicitly fill with bgDark so any semi-transparent edge pixels blend
        // against the exact same colour as the rest of the transport bar.
        g.setColour(juce::Colour(LookAndFeel_BlockShuffler::bgDark));
        g.fillRect(brandingArea);

        float scale = juce::jmin((float)brandingArea.getHeight() / (float)logoImage.getHeight(),
                                 (float)brandingArea.getWidth()  / (float)logoImage.getWidth());
        int drawW = juce::roundToInt((float)logoImage.getWidth()  * scale);
        int drawH = juce::roundToInt((float)logoImage.getHeight() * scale);
        int drawX = brandingArea.getCentreX() - drawW / 2;
        int drawY = brandingArea.getCentreY() - drawH / 2;
        g.drawImage(logoImage, drawX, drawY, drawW, drawH, 0, 0,
                    logoImage.getWidth(), logoImage.getHeight());
    }
}

void TransportBar::resized() {
    auto area = getLocalBounds().reduced(8, 6);
    const int gap  = 4;
    const int btnH = 28;

    // Left: transport controls
    rewindBtn.setBounds(area.removeFromLeft(36).withSizeKeepingCentre(36, btnH));
    area.removeFromLeft(gap);
    playBtn  .setBounds(area.removeFromLeft(68).withSizeKeepingCentre(68, btnH));
    area.removeFromLeft(gap);
    stopBtn  .setBounds(area.removeFromLeft(60).withSizeKeepingCentre(60, btnH));
    area.removeFromLeft(gap * 4);
    exportBtn.setBounds(area.removeFromLeft(72).withSizeKeepingCentre(72, btnH));

    // Right: file buttons then logo just to their left
    openBtn  .setBounds(area.removeFromRight(60).withSizeKeepingCentre(60, btnH));
    area.removeFromRight(gap);
    saveBtn  .setBounds(area.removeFromRight(60).withSizeKeepingCentre(60, btnH));
    area.removeFromRight(gap);
    saveAsBtn.setBounds(area.removeFromRight(68).withSizeKeepingCentre(68, btnH));
    area.removeFromRight(12);  // gap between Save As and logo

    // Logo: client spec 12.3 — drawn at ~2/3 of the FULL bar height (not the
    // inset area), centred vertically, immediately left of the Save As button
    const int logoH = juce::roundToInt((float)getLocalBounds().getHeight() * 2.0f / 3.0f);
    int logoW = 0;
    if (logoImage.isValid() && logoImage.getHeight() > 0)
        logoW = juce::roundToInt((float)logoH * (float)logoImage.getWidth()
                                              / (float)logoImage.getHeight());
    logoW = juce::jlimit(0, 180, logoW);
    brandingArea = (logoW > 0) ? area.removeFromRight(logoW).withSizeKeepingCentre(logoW, logoH)
                               : juce::Rectangle<int>();

    // Remaining centre strip = time display
    area.removeFromLeft(gap * 2);
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
