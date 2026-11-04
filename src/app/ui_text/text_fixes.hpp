#pragma once

#include <cstdint>

namespace rr::app {

/// FirstGlyph draws the original's fallback: its lowest-coded glyph, or else its space cell.
enum class MissingGlyphs : std::uint8_t { FirstGlyph, Shapes };

struct TextFixes {
    MissingGlyphs missingGlyphs = MissingGlyphs::FirstGlyph;
    bool fixesGlyphLists = false;
};

}
