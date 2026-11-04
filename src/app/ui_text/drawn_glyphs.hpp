#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "render/render_view.hpp"

namespace rr::app {

enum class DrawnGlyph : std::uint8_t { Dash, Slash };

/// FirstGlyph draws the original's fallback: its lowest-coded glyph, or else its space cell.
enum class MissingGlyphs : std::uint8_t { FirstGlyph, Shapes };

/// Punctuation drawn as a shape when a font has no glyph for it.
[[nodiscard]] std::optional<DrawnGlyph> DrawnGlyphFor(char16_t character);

/// In font pixels, for a font `heightPx` tall.
[[nodiscard]] float DrawnGlyphWidth(DrawnGlyph glyph, float heightPx);

/// `cell` is the glyph's box on screen, as tall as the font.
void DrawGlyphShape(DrawnGlyph glyph, const render::Rect& cell, const render::Color& tint,
                    std::vector<render::SpriteDraw>& out);

}
