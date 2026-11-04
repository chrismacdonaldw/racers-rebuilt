#include "app/ui_text/drawn_glyphs.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace rr::app {
namespace {

constexpr float kDashWidth = 0.45F;
constexpr float kSlashWidth = 0.5F;
constexpr float kStroke = 0.14F;
constexpr float kDashMiddle = 0.55F;
constexpr float kSlashTop = 0.1F;
constexpr float kSlashBottom = 0.9F;

}

std::optional<DrawnGlyph> DrawnGlyphFor(char16_t character) {
    switch (character) {
        case u'-':
            return DrawnGlyph::Dash;
        case u'/':
            return DrawnGlyph::Slash;
        default:
            return std::nullopt;
    }
}

float DrawnGlyphWidth(DrawnGlyph glyph, float heightPx) {
    return std::round(heightPx * (glyph == DrawnGlyph::Dash ? kDashWidth : kSlashWidth));
}

void DrawGlyphShape(DrawnGlyph glyph, const render::Rect& cell, const render::Color& tint,
                    std::vector<render::SpriteDraw>& out) {
    const float stroke = std::max(cell.height * kStroke, 1.0F);
    if (glyph == DrawnGlyph::Dash) {
        out.push_back({.bounds = {.x = cell.x,
                                  .y = cell.y + (cell.height * kDashMiddle) - (stroke / 2.0F),
                                  .width = cell.width,
                                  .height = stroke},
                       .tint = tint});
        return;
    }
    const float top = cell.y + (cell.height * kSlashTop);
    const float bottom = cell.y + (cell.height * kSlashBottom);
    const float right = cell.x + cell.width;
    const auto corner = [&](float x, float y) {
        return render::SpriteCorner{.x = x, .y = y, .tint = tint};
    };
    out.push_back({.corners = std::array{corner(right - stroke, top), corner(right, top),
                                         corner(cell.x, bottom), corner(cell.x + stroke, bottom)}});
}

}
