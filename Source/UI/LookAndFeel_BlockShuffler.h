#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace BlockShuffler {

/**
 * Dark theme matching the mockup aesthetic.
 * See CLAUDE.md "Color Palette" for exact values.
 */
class LookAndFeel_BlockShuffler : public juce::LookAndFeel_V4 {
public:
    // Core palette
    static constexpr juce::uint32 bgDark        = 0xFF1E1E1E;
    static constexpr juce::uint32 bgMedium      = 0xFF2D2D2D;
    static constexpr juce::uint32 bgLight       = 0xFF3C3C3C;
    static constexpr juce::uint32 waveformFill   = 0xFFCCCCCC;
    static constexpr juce::uint32 gridLineColor  = 0xFF4A4A4A;
    static constexpr juce::uint32 startMarkerCol = 0xFF00CC00;
    static constexpr juce::uint32 endMarkerCol   = 0xFFCC0000;
    static constexpr juce::uint32 playheadCol    = 0xFFFF3333;
    static constexpr juce::uint32 textPrimary    = 0xFFE0E0E0;
    static constexpr juce::uint32 textSecondary  = 0xFF999999;
    static constexpr juce::uint32 accentCol      = 0xFF5599FF;

    LookAndFeel_BlockShuffler();
    ~LookAndFeel_BlockShuffler() override = default;

    // Default block colors
    static juce::Array<juce::Colour> getBlockPalette();
};

} // namespace BlockShuffler
