#pragma once

#include <string_view>
#include <vector>

#include "app/ui_text/ui_font.hpp"
#include "core/bounding_sphere.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// Frames `bounds` from above the front corner in a square tile.
[[nodiscard]] render::Camera PieceTileCamera(const BoundingSphere& bounds);

/// A frame of 2 units just outside `area`.
void DrawOutline(const render::Rect& area, const render::Color& colour,
                 std::vector<render::SpriteDraw>& out);

/// Centres `text` in `area`, shrunk to fit. Draws nothing without a font.
void DrawFittedText(const UiFont* font, std::u16string_view text, const render::Rect& area,
                    const render::Color& tint, std::vector<render::SpriteDraw>& out);

}
