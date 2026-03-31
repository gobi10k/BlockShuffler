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

    // Buttons
    setColour(juce::TextButton::buttonColourId, juce::Colour(bgLight));
    setColour(juce::TextButton::textColourOffId, juce::Colour(textPrimary));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(accentCol));

    // Sliders
    setColour(juce::Slider::thumbColourId, juce::Colour(accentCol));
    setColour(juce::Slider::trackColourId, juce::Colour(bgLight));
    setColour(juce::Slider::backgroundColourId, juce::Colour(bgMedium));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(textPrimary));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(bgLight));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(gridLineColor));

    // Scrollbar
    setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xFF555555));
    setColour(juce::ScrollBar::backgroundColourId, juce::Colour(bgMedium));

    // ComboBox
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(bgLight));
    setColour(juce::ComboBox::textColourId, juce::Colour(textPrimary));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(gridLineColor));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(textSecondary));

    // PopupMenu
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(bgMedium));
    setColour(juce::PopupMenu::textColourId, juce::Colour(textPrimary));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(accentCol));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    // ToggleButton / Checkbox
    setColour(juce::ToggleButton::textColourId, juce::Colour(textPrimary));
    setColour(juce::ToggleButton::tickColourId, juce::Colour(accentCol));
    setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(textSecondary));

    // Tooltip
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(bgLight));
    setColour(juce::TooltipWindow::textColourId, juce::Colour(textPrimary));
    setColour(juce::TooltipWindow::outlineColourId, juce::Colour(gridLineColor));
}

juce::Array<juce::Colour> LookAndFeel_BlockShuffler::getBlockPalette() {
    return {
        juce::Colour(0xFFCC4444),  // Red
        juce::Colour(0xFFCC8844),  // Orange
        juce::Colour(0xFFCCAA44),  // Yellow
        juce::Colour(0xFF44CC44),  // Green
        juce::Colour(0xFF44CCCC),  // Cyan
        juce::Colour(0xFF4488CC),  // Blue
        juce::Colour(0xFF8844CC),  // Purple
        juce::Colour(0xFFCC44AA),  // Pink
    };
}

} // namespace BlockShuffler
