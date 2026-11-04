#pragma once

#include <vector>

#include "core/bounding_sphere.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// Frames `bounds` from above the front corner in a square tile.
[[nodiscard]] render::Camera PieceTileCamera(const BoundingSphere& bounds);

/// A frame of 2 units just outside `area`.
void DrawOutline(const render::Rect& area, const render::Color& colour,
                 std::vector<render::SpriteDraw>& out);

}
