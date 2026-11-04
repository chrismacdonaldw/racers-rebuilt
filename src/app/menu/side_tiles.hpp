#pragma once

#include <vector>

#include "app/menu/menu_parts.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// Fills the bars beside a wider window with copies of a full-height picture on the canvas's side,
/// mirrored and plain in turn outward from that side.
void AppendSideTiles(const MenuPicture& picture, std::vector<render::SpriteDraw>& out);

}
