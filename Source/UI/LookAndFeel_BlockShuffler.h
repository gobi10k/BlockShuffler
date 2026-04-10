#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace BlockShuffler {

/**
 * BlockShuffler brand palette — dark precision + neon energy + modular clarity.
 * Generated from the BlockShuffler logo aesthetic brief (April 2026).
 */
class LookAndFeel_BlockShuffler : public juce::LookAndFeel_V4 {
public:
    // ── Backgrounds ───────────────────────────────────────────────────────────
    static constexpr juce::uint32 bgDark        = 0xFF0B0D10;  // Void Black
    static constexpr juce::uint32 bgMedium      = 0xFF121825;  // Graphite Blue
    static constexpr juce::uint32 bgLight       = 0xFF1C2533;  // Slate Steel
    static constexpr juce::uint32 panelCol      = 0xFF24324A;  // Deep Navy

    // ── Text ──────────────────────────────────────────────────────────────────
    static constexpr juce::uint32 textPrimary   = 0xFFF4F7FB;  // Soft White
    static constexpr juce::uint32 textSecondary = 0xFFA7B0C0;  // Cool Gray

    // ── Accents (use sparingly against the dark base) ─────────────────────────
    static constexpr juce::uint32 accentCol     = 0xFF25D7F2;  // Neon Cyan      — primary
    static constexpr juce::uint32 accentViolet  = 0xFF7B5CFF;  // Electric Violet
    static constexpr juce::uint32 accentMagenta = 0xFFFF4FB8;  // Hot Magenta
    static constexpr juce::uint32 accentLime    = 0xFFA6F46A;  // Signal Lime
    static constexpr juce::uint32 accentAmber   = 0xFFFFB347;  // Amber Pulse
    static constexpr juce::uint32 accentCoral   = 0xFFFF6B6B;  // Infrared Coral
    static constexpr juce::uint32 accentIceBlue = 0xFF7EDCFF;  // Ice Blue
    static constexpr juce::uint32 accentTeal    = 0xFF20C997;  // Acid Teal

    // ── Functional / audio UI ─────────────────────────────────────────────────
    static constexpr juce::uint32 waveformFill   = 0xFF7EDCFF;  // Ice Blue waveform
    static constexpr juce::uint32 gridLineColor  = 0xFF1E2E45;  // subtle grid on panels
    static constexpr juce::uint32 startMarkerCol = 0xFFA6F46A;  // Signal Lime  (green ≈ start)
    static constexpr juce::uint32 endMarkerCol   = 0xFFFF6B6B;  // Infrared Coral (red ≈ end)
    static constexpr juce::uint32 playheadCol    = 0xFFFF4FB8;  // Hot Magenta playhead

    LookAndFeel_BlockShuffler();
    ~LookAndFeel_BlockShuffler() override = default;

    // Default block colors — full accent palette
    static juce::Array<juce::Colour> getBlockPalette();
};

} // namespace BlockShuffler
