#include "LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

LookAndFeel_BlockShuffler::LookAndFeel_BlockShuffler() {
    // Window / background
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(bgDark));
    setColour(juce::DocumentWindow::backgroundColourId,  juce::Colour(bgDark));

    // Text
    setColour(juce::Label::textColourId, juce::Colour(textPrimary));
    setColour(juce::TextEditor::textColourId, juce::Colour(textPrimary));
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(bgLight));
    setColour(juce::TextEditor::outlineColourId, juce::Colour(gridLineColor));
    setColour(juce::TextEditor::highlightColourId, juce::Colour(accentCol).withAlpha(0.4f));
    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(accentCol).withAlpha(0.7f));

    // Buttons — surface + accent highlight
    setColour(juce::TextButton::buttonColourId,   juce::Colour(bgLight));
    setColour(juce::TextButton::textColourOffId,  juce::Colour(textPrimary));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(accentCol).withAlpha(0.25f));
    setColour(juce::TextButton::textColourOnId,   juce::Colour(accentCol));

    // Sliders — neon cyan thumb
    setColour(juce::Slider::thumbColourId,              juce::Colour(accentCol));
    setColour(juce::Slider::trackColourId,              juce::Colour(accentCol).withAlpha(0.35f));
    setColour(juce::Slider::backgroundColourId,         juce::Colour(bgMedium));
    setColour(juce::Slider::textBoxTextColourId,        juce::Colour(textPrimary));
    setColour(juce::Slider::textBoxBackgroundColourId,  juce::Colour(bgLight));
    setColour(juce::Slider::textBoxOutlineColourId,     juce::Colour(gridLineColor));

    // Scrollbar — subtle steel thumb
    setColour(juce::ScrollBar::thumbColourId,       juce::Colour(panelCol));
    setColour(juce::ScrollBar::backgroundColourId,  juce::Colour(bgMedium));

    // ComboBox
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(bgLight));
    setColour(juce::ComboBox::textColourId,       juce::Colour(textPrimary));
    setColour(juce::ComboBox::outlineColourId,    juce::Colour(gridLineColor));
    setColour(juce::ComboBox::arrowColourId,      juce::Colour(textSecondary));

    // PopupMenu — deep navy surface, cyan highlight
    setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(bgMedium));
    setColour(juce::PopupMenu::textColourId,                  juce::Colour(textPrimary));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(accentCol).withAlpha(0.2f));
    setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(accentCol));
    setColour(juce::PopupMenu::headerTextColourId,            juce::Colour(textSecondary));

    // ToggleButton / Checkbox
    setColour(juce::ToggleButton::textColourId,         juce::Colour(textPrimary));
    setColour(juce::ToggleButton::tickColourId,         juce::Colour(accentCol));
    setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(textSecondary));

    // Tooltip
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(panelCol));
    setColour(juce::TooltipWindow::textColourId,       juce::Colour(textPrimary));
    setColour(juce::TooltipWindow::outlineColourId,    juce::Colour(accentCol).withAlpha(0.4f));
}

juce::Array<juce::Colour> LookAndFeel_BlockShuffler::getBlockPalette() {
    return {
        juce::Colour(accentCoral),   // Infrared Coral
        juce::Colour(accentAmber),   // Amber Pulse
        juce::Colour(accentLime),    // Signal Lime
        juce::Colour(accentTeal),    // Acid Teal
        juce::Colour(accentCol),     // Neon Cyan
        juce::Colour(accentIceBlue), // Ice Blue
        juce::Colour(accentViolet),  // Electric Violet
        juce::Colour(accentMagenta), // Hot Magenta
    };
}

} // namespace BlockShuffler
