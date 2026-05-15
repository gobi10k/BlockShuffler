#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace BlockShuffler {

/**
 * BlockShuffler LookAndFeel — layered dark + single teal accent.
 * All components should source colours via the ColourIds enum (findColour)
 * or the static constants for direct paint() use.
 */
class LookAndFeel_BlockShuffler : public juce::LookAndFeel_V4 {
public:
    // ── Semantic colour IDs (use via findColour() in draw* overrides) ─────────
    enum ColourIds : int {
        backgroundBaseId     = 0x00B50000,
        backgroundPanelId    = 0x00B50001,
        backgroundElevatedId = 0x00B50002,
        backgroundHoverId    = 0x00B50003,
        accentId             = 0x00B50004,
        accentMutedId        = 0x00B50005,
        textPrimaryId        = 0x00B50006,
        textSecondaryId      = 0x00B50007,
        textTertiaryId       = 0x00B50008,
        borderSubtleId       = 0x00B50009,
        borderStrongId       = 0x00B5000A,
    };

    // ── Backgrounds ───────────────────────────────────────────────────────────
    static constexpr juce::uint32 bgDark   = 0xFF0E1216;  // Base
    static constexpr juce::uint32 bgMedium = 0xFF161B22;  // Panel
    static constexpr juce::uint32 bgLight  = 0xFF1F2630;  // Elevated
    static constexpr juce::uint32 panelCol = 0xFF2A3340;  // Hover / raised

    // ── Text ──────────────────────────────────────────────────────────────────
    static constexpr juce::uint32 textPrimary   = 0xFFE8EEF2;
    static constexpr juce::uint32 textSecondary = 0xFF8B96A4;
    static constexpr juce::uint32 textTertiary  = 0xFF5A6473;

    // ── Borders ───────────────────────────────────────────────────────────────
    static constexpr juce::uint32 borderSubtle = 0xFF242B35;
    static constexpr juce::uint32 borderStrong = 0xFF323B47;

    // ── Primary accent (teal-cyan, locked in) ─────────────────────────────────
    static constexpr juce::uint32 accentCol   = 0xFF4DD6D1;
    static constexpr juce::uint32 accentMuted = 0xFF2D9E9A;

    // ── Functional / audio UI ─────────────────────────────────────────────────
    static constexpr juce::uint32 waveformFill   = 0xFFCCCCCC;  // light gray
    static constexpr juce::uint32 gridLineColor  = 0xFF242B35;  // borderSubtle
    static constexpr juce::uint32 startMarkerCol = 0xFF4CC87A;  // green
    static constexpr juce::uint32 endMarkerCol   = 0xFFE86060;  // coral red
    static constexpr juce::uint32 playheadCol    = 0xFFFF3333;  // red playhead

    // ── Block identity palette — also used for UI accents ────────────────────
    // getBlockPalette() / getBlockColour() expose the user-assignable subset.
    static constexpr juce::uint32 accentCoral   = 0xFFD45C5C;  // Coral Red
    static constexpr juce::uint32 accentAmber   = 0xFFD4883C;  // Amber Orange
    static constexpr juce::uint32 accentLime    = 0xFF7FC43C;  // Lime Green  (UI accent only)
    static constexpr juce::uint32 accentTeal    = 0xFF2DB88A;  // Mint Teal   (UI accent only)
    static constexpr juce::uint32 accentIceBlue = 0xFF3C94D4;  // Sky Blue    (UI accent only)
    static constexpr juce::uint32 accentViolet  = 0xFF6868CC;  // Indigo      (app icon)
    static constexpr juce::uint32 accentPurple  = 0xFF9060CC;  // Violet
    static constexpr juce::uint32 accentMagenta = 0xFFCC4E90;  // Rose Pink   (app icon)

    // ── User-palette hues — named to match what they look like ───────────────
    static constexpr juce::uint32 paletteRed    = 0xFFE05252;  // Red
    static constexpr juce::uint32 paletteOrange = 0xFFE08840;  // Orange
    static constexpr juce::uint32 paletteYellow = 0xFFD4C840;  // Yellow
    static constexpr juce::uint32 paletteGreen  = 0xFF4CC840;  // Green
    static constexpr juce::uint32 paletteCyan   = 0xFF40C0D4;  // Cyan
    static constexpr juce::uint32 paletteBlue   = 0xFF4488D4;  // Blue
    static constexpr juce::uint32 palettePurple = 0xFF9060CC;  // Purple
    static constexpr juce::uint32 palettePink   = 0xFFCC4E90;  // Pink

    LookAndFeel_BlockShuffler();
    ~LookAndFeel_BlockShuffler() override = default;

    // ── Font routing ──────────────────────────────────────────────────────────
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& f) override;

    /** Returns Inter-Regular at the requested size — UI labels. */
    static juce::Font uiFont(float size);
    /** Returns Inter-Bold at the requested size — headings / section titles. */
    static juce::Font uiFontBold(float size);
    /** Returns JetBrains Mono at the requested size — numeric readouts. */
    static juce::Font monoFont(float size);

    // ── Block colour helpers ───────────────────────────────────────────────────
    static juce::Array<juce::Colour> getBlockPalette();
    static juce::Colour              getBlockColour(int index);

    // ── Custom draw overrides ─────────────────────────────────────────────────

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawTickBox(juce::Graphics& g, juce::Component& component,
                     float x, float y, float w, float h,
                     bool ticked, bool isEnabled,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

    void drawTooltip(juce::Graphics& g, const juce::String& text,
                     int width, int height) override;

    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;

    /** Measure the advance width of a single line of text.
     *  Replaces the deprecated juce::Font::getStringWidthFloat(). */
    static float measureTextWidth(const juce::Font& font, const juce::String& text) {
        juce::GlyphArrangement ga;
        ga.addLineOfText(font, text, 0.0f, 0.0f);
        return ga.getNumGlyphs() > 0 ? ga.getBoundingBox(0, -1, true).getWidth() : 0.0f;
    }

private:
    juce::Typeface::Ptr interRegular;
    juce::Typeface::Ptr interBold;
    juce::Typeface::Ptr jetbrainsMono;
};

} // namespace BlockShuffler
