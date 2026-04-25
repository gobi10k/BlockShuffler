#include "LookAndFeel_BlockShuffler.h"
#include <BinaryData.h>

namespace BlockShuffler {

LookAndFeel_BlockShuffler::LookAndFeel_BlockShuffler() {
    // Load typefaces from embedded binary data
    interRegular  = juce::Typeface::createSystemTypefaceFor(
                        BinaryData::Inter_Regular_ttf,
                        BinaryData::Inter_Regular_ttfSize);
    interBold     = juce::Typeface::createSystemTypefaceFor(
                        BinaryData::Inter_Bold_ttf,
                        BinaryData::Inter_Bold_ttfSize);
    jetbrainsMono = juce::Typeface::createSystemTypefaceFor(
                        BinaryData::JetBrainsMono_Regular_ttf,
                        BinaryData::JetBrainsMono_Regular_ttfSize);

    // ── Semantic colour IDs ───────────────────────────────────────────────────
    setColour(backgroundBaseId,     juce::Colour(bgDark));
    setColour(backgroundPanelId,    juce::Colour(bgMedium));
    setColour(backgroundElevatedId, juce::Colour(bgLight));
    setColour(backgroundHoverId,    juce::Colour(panelCol));
    setColour(accentId,             juce::Colour(accentCol));
    setColour(accentMutedId,        juce::Colour(accentMuted));
    setColour(textPrimaryId,        juce::Colour(textPrimary));
    setColour(textSecondaryId,      juce::Colour(textSecondary));
    setColour(textTertiaryId,       juce::Colour(textTertiary));
    setColour(borderSubtleId,       juce::Colour(borderSubtle));
    setColour(borderStrongId,       juce::Colour(borderStrong));

    // ── Window / background ───────────────────────────────────────────────────
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(bgDark));
    setColour(juce::DocumentWindow::backgroundColourId,  juce::Colour(bgDark));

    // ── Text / labels ─────────────────────────────────────────────────────────
    setColour(juce::Label::textColourId,               juce::Colour(textPrimary));
    setColour(juce::TextEditor::textColourId,           juce::Colour(textPrimary));
    setColour(juce::TextEditor::backgroundColourId,     juce::Colour(bgLight));
    setColour(juce::TextEditor::outlineColourId,        juce::Colour(borderSubtle));
    setColour(juce::TextEditor::highlightColourId,      juce::Colour(accentCol).withAlpha(0.35f));
    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(accentCol).withAlpha(0.75f));

    // ── Buttons ───────────────────────────────────────────────────────────────
    setColour(juce::TextButton::buttonColourId,   juce::Colour(bgLight));
    setColour(juce::TextButton::textColourOffId,  juce::Colour(textPrimary));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(accentCol).withAlpha(0.2f));
    setColour(juce::TextButton::textColourOnId,   juce::Colour(accentCol));

    // ── Sliders ───────────────────────────────────────────────────────────────
    setColour(juce::Slider::thumbColourId,             juce::Colour(accentCol));
    setColour(juce::Slider::trackColourId,             juce::Colour(accentCol).withAlpha(0.4f));
    setColour(juce::Slider::backgroundColourId,        juce::Colour(borderSubtle));
    setColour(juce::Slider::textBoxTextColourId,       juce::Colour(textPrimary));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(bgLight));
    setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(borderSubtle));

    // ── Scrollbars ────────────────────────────────────────────────────────────
    setColour(juce::ScrollBar::thumbColourId,      juce::Colour(borderStrong));
    setColour(juce::ScrollBar::backgroundColourId, juce::Colour(bgMedium));

    // ── ComboBox ──────────────────────────────────────────────────────────────
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(bgLight));
    setColour(juce::ComboBox::textColourId,       juce::Colour(textPrimary));
    setColour(juce::ComboBox::outlineColourId,    juce::Colour(borderSubtle));
    setColour(juce::ComboBox::arrowColourId,      juce::Colour(textSecondary));

    // ── PopupMenu ─────────────────────────────────────────────────────────────
    setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(bgMedium));
    setColour(juce::PopupMenu::textColourId,                  juce::Colour(textPrimary));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(accentCol).withAlpha(0.18f));
    setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(accentCol));
    setColour(juce::PopupMenu::headerTextColourId,            juce::Colour(textSecondary));

    // ── ToggleButton / Checkbox ───────────────────────────────────────────────
    setColour(juce::ToggleButton::textColourId,         juce::Colour(textPrimary));
    setColour(juce::ToggleButton::tickColourId,         juce::Colour(accentCol));
    setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(textTertiary));

    // ── Tooltip ───────────────────────────────────────────────────────────────
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(bgLight));
    setColour(juce::TooltipWindow::textColourId,       juce::Colour(textPrimary));
    setColour(juce::TooltipWindow::outlineColourId,    juce::Colour(borderStrong));
}

// ── Font routing ──────────────────────────────────────────────────────────────

juce::Typeface::Ptr LookAndFeel_BlockShuffler::getTypefaceForFont(const juce::Font& f) {
    // Route monospace requests to JetBrains Mono
    if (f.getTypefaceName() == "JetBrainsMono" ||
        f.getTypefaceName() == "Monospace" ||
        f.getTypefaceName() == "Courier New")
        return jetbrainsMono;

    // Bold weight → Inter Bold
    if (f.isBold() || f.getTypefaceName() == "Inter-Bold")
        return interBold;

    // Default → Inter Regular
    return interRegular;
}

juce::Font LookAndFeel_BlockShuffler::uiFont(float size) {
    return juce::Font(juce::FontOptions().withName("Inter").withHeight(size));
}

juce::Font LookAndFeel_BlockShuffler::uiFontBold(float size) {
    return juce::Font(juce::FontOptions().withName("Inter-Bold").withHeight(size).withStyle("Bold"));
}

juce::Font LookAndFeel_BlockShuffler::monoFont(float size) {
    return juce::Font(juce::FontOptions().withName("JetBrainsMono").withHeight(size));
}

// ── Block colour helpers ──────────────────────────────────────────────────────

juce::Array<juce::Colour> LookAndFeel_BlockShuffler::getBlockPalette() {
    return {
        juce::Colour(accentCoral),   // Coral Red
        juce::Colour(accentAmber),   // Amber Orange
        juce::Colour(accentLime),    // Lime Green
        juce::Colour(accentTeal),    // Mint Teal
        juce::Colour(accentIceBlue), // Sky Blue
        juce::Colour(accentViolet),  // Indigo
        juce::Colour(accentPurple),  // Violet
        juce::Colour(accentMagenta), // Rose Pink
    };
}

juce::Colour LookAndFeel_BlockShuffler::getBlockColour(int index) {
    auto palette = getBlockPalette();
    if (palette.isEmpty()) return juce::Colour(accentCol);
    return palette[((index % palette.size()) + palette.size()) % palette.size()];
}

} // namespace BlockShuffler
