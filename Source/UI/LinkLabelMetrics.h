#pragma once
// ── LinkLabelMetrics (2026-08-30, Carter Windows label-overlap round) ────────
// THE SINGLE DEFINITION of how wide a link label is.
//
// WHY THIS FILE EXISTS. BlockLinkOverlay::paint measured a label with one font
// and the diag tests asserted against numbers typed into the test (labelW=96,
// pillW=34, "~ 'Block 3 <-> Block 4' at 10pt"). Those constants were measured on
// a Mac and then frozen, so the tests could only ever prove the layout was
// self-consistent for THOSE widths — they could not see a platform whose real
// widths were different, which is exactly how T63/T65 stayed green on Windows CI
// while Carter watched the labels overlap on his screen.
//
// So the collision box and the drawn glyphs now come from the same font object,
// the same text, the same measurement call and the same padding — here. A test
// that wants a label width has to ask for it the way paint() does; there is no
// longer a number for it to invent.

#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel_BlockShuffler.h"

namespace BlockShuffler {
namespace LinkLabelMetrics {

/** Padding baked into each row's box, around the measured glyph run. */
inline constexpr float namePadding = 10.0f;
inline constexpr float pillPadding = 12.0f;

/** The two fonts the overlay draws with. Call these; do not rebuild a Font by
 *  hand, or the box and the glyphs can drift apart again. */
inline juce::Font nameFont() { return LookAndFeel_BlockShuffler::uiFont(10.0f); }
inline juce::Font pillFont() { return LookAndFeel_BlockShuffler::monoFont(10.0f); }

inline juce::String nameText(const juce::String& blockA, const juce::String& blockB) {
    return blockA + " <-> " + blockB;
}
inline juce::String pillText(float swapProbability) {
    return juce::String((int)(swapProbability * 100.0f)) + "%";
}

/** Width of the name row's box: measured advance of the real glyph run in the
 *  real font, plus the padding the plate is drawn with. */
inline float nameWidth(const juce::Font& font, const juce::String& text) {
    return LookAndFeel_BlockShuffler::measureTextWidth(font, text) + namePadding;
}
/** Width of the probability pill's box, measured the same way. */
inline float pillWidth(const juce::Font& font, const juce::String& text) {
    return LookAndFeel_BlockShuffler::measureTextWidth(font, text) + pillPadding;
}

} // namespace LinkLabelMetrics
} // namespace BlockShuffler
