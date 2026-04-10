#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace BlockShuffler {

/**
 * RiverMix brand palette — extracted from the RiverMix wordmark and icon.
 *
 * Foundation: pure black backgrounds, white text, and a single
 * blue→cyan gradient as the signature accent.  No rainbow palette.
 *
 * Gradient: Deep Blue #1E40FF → Mid Blue #2B7FFF → Cyan #00C8FF
 */
class LookAndFeel_BlockShuffler : public juce::LookAndFeel_V4 {
public:
    // ── Backgrounds (near-black family) ───────────────────────────────────────
    static constexpr juce::uint32 bgDark        = 0xFF000000;  // Pure Black
    static constexpr juce::uint32 bgMedium      = 0xFF080810;  // Near Black
    static constexpr juce::uint32 bgLight       = 0xFF10101C;  // Very Dark Blue-Black
    static constexpr juce::uint32 panelCol      = 0xFF181828;  // Dark Panel

    // ── Text ──────────────────────────────────────────────────────────────────
    static constexpr juce::uint32 textPrimary   = 0xFFFFFFFF;  // Pure White  (matches "River")
    static constexpr juce::uint32 textSecondary = 0xFF8B96B0;  // Cool Gray

    // ── Brand gradient (use accentCyan as the "hot" highlight) ────────────────
    static constexpr juce::uint32 accentCyan    = 0xFF00C8FF;  // Electric Cyan  — gradient top
    static constexpr juce::uint32 accentMid     = 0xFF2B7FFF;  // Mid Blue       — gradient mid
    static constexpr juce::uint32 accentDeep    = 0xFF1E40FF;  // Deep Blue      — gradient base

    // ── Functional / audio UI ─────────────────────────────────────────────────
    static constexpr juce::uint32 waveformFill   = 0xFF00C8FF;  // Cyan waveform
    static constexpr juce::uint32 gridLineColor  = 0xFF1A1A2E;  // Very subtle grid
    static constexpr juce::uint32 startMarkerCol = 0xFF00C8FF;  // Cyan  (bright / positive)
    static constexpr juce::uint32 endMarkerCol   = 0xFFFFFFFF;  // White (clean / end)
    static constexpr juce::uint32 playheadCol    = 0xFF2B7FFF;  // Mid Blue playhead

    // Convenience aliases used by older paint() callsites
    static constexpr juce::uint32 accentCol     = accentCyan;   // primary accent = Cyan

    LookAndFeel_BlockShuffler();
    ~LookAndFeel_BlockShuffler() override = default;

    // Block colors — cool-blue family matching the brand gradient
    static juce::Array<juce::Colour> getBlockPalette();
};

} // namespace BlockShuffler
