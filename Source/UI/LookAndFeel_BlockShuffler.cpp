#include "LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {

LookAndFeel_BlockShuffler::LookAndFeel_BlockShuffler() {
    // Window / background — pure black
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(bgDark));
    setColour(juce::DocumentWindow::backgroundColourId,  juce::Colour(bgDark));

    // Text — white primary, cool-gray secondary
    setColour(juce::Label::textColourId,                   juce::Colour(textPrimary));
    setColour(juce::TextEditor::textColourId,              juce::Colour(textPrimary));
    setColour(juce::TextEditor::backgroundColourId,        juce::Colour(bgLight));
    setColour(juce::TextEditor::outlineColourId,           juce::Colour(gridLineColor));
    setColour(juce::TextEditor::highlightColourId,         juce::Colour(accentCyan).withAlpha(0.35f));
    setColour(juce::TextEditor::focusedOutlineColourId,    juce::Colour(accentCyan).withAlpha(0.7f));

    // Buttons — dark surface, cyan text/glow on hover/on
    setColour(juce::TextButton::buttonColourId,   juce::Colour(bgLight));
    setColour(juce::TextButton::textColourOffId,  juce::Colour(textPrimary));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(accentDeep).withAlpha(0.4f));
    setColour(juce::TextButton::textColourOnId,   juce::Colour(accentCyan));

    // Sliders — cyan thumb, mid-blue track
    setColour(juce::Slider::thumbColourId,             juce::Colour(accentCyan));
    setColour(juce::Slider::trackColourId,             juce::Colour(accentMid).withAlpha(0.5f));
    setColour(juce::Slider::backgroundColourId,        juce::Colour(bgMedium));
    setColour(juce::Slider::textBoxTextColourId,       juce::Colour(textPrimary));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(bgLight));
    setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(gridLineColor));

    // Scrollbar
    setColour(juce::ScrollBar::thumbColourId,      juce::Colour(panelCol));
    setColour(juce::ScrollBar::backgroundColourId, juce::Colour(bgMedium));

    // ComboBox
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(bgLight));
    setColour(juce::ComboBox::textColourId,       juce::Colour(textPrimary));
    setColour(juce::ComboBox::outlineColourId,    juce::Colour(gridLineColor));
    setColour(juce::ComboBox::arrowColourId,      juce::Colour(textSecondary));

    // PopupMenu — near-black surface, cyan highlight
    setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(bgMedium));
    setColour(juce::PopupMenu::textColourId,                  juce::Colour(textPrimary));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(accentDeep).withAlpha(0.45f));
    setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(accentCyan));
    setColour(juce::PopupMenu::headerTextColourId,            juce::Colour(textSecondary));

    // ToggleButton / Checkbox
    setColour(juce::ToggleButton::textColourId,         juce::Colour(textPrimary));
    setColour(juce::ToggleButton::tickColourId,         juce::Colour(accentCyan));
    setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(textSecondary));

    // Tooltip — panel bg, cyan outline
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(panelCol));
    setColour(juce::TooltipWindow::textColourId,       juce::Colour(textPrimary));
    setColour(juce::TooltipWindow::outlineColourId,    juce::Colour(accentCyan).withAlpha(0.5f));
}

juce::Array<juce::Colour> LookAndFeel_BlockShuffler::getBlockPalette() {
    // Cool-blue family derived from the RiverMix brand gradient
    return {
        juce::Colour(0xFF00C8FF),  // Cyan            (gradient top)
        juce::Colour(0xFF2B7FFF),  // Mid Blue         (gradient mid)
        juce::Colour(0xFF1E40FF),  // Deep Blue        (gradient base)
        juce::Colour(0xFF5EB4FF),  // Sky Blue
        juce::Colour(0xFF00A0D8),  // Dark Cyan
        juce::Colour(0xFF6B8CFF),  // Periwinkle
        juce::Colour(0xFF0060D8),  // Strong Blue
        juce::Colour(0xFF90E0FF),  // Pale Cyan
    };
}

} // namespace BlockShuffler
