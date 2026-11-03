#pragma once

#include <span>
#include <vector>

#include "app/menu/menu_parts.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// The pictures' parts inside `hole`, as quads behind all else `camera` sees when drawn into it.
void AppendBackdropQuads(std::span<const MenuPicture> pictures, const render::Rect& hole,
                         const render::Camera& camera, std::vector<render::QuadDraw>& out);

}
