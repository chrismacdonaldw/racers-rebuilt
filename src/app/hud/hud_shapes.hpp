#pragma once

#include <array>
#include <vector>

#include "render/render_view.hpp"

namespace rr::app {

using HudTriangle = std::array<render::SpriteCorner, 3>;

/// A cut edge takes the colour of the corner it cut away, as the original HUD's clipper does.
void AppendClipped(const HudTriangle& triangle, const render::Rect& clip, render::TextureId texture,
                   std::vector<render::SpriteDraw>& out);

void AppendTriangle(const HudTriangle& triangle, render::TextureId texture,
                    std::vector<render::SpriteDraw>& out);

}
