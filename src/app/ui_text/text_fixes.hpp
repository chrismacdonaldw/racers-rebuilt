#pragma once

#include <cstdint>

namespace rr::app {

/// FirstGlyph draws the original's fallback; Shapes folds lower case and draws - and / as shapes.
enum class MissingGlyphs : std::uint8_t { FirstGlyph, Shapes };

struct TextFixes {
    MissingGlyphs missingGlyphs = MissingGlyphs::FirstGlyph;
    bool fixesGlyphLists = false;
};

}
