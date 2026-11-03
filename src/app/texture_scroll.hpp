#pragma once

#include <span>

#include "app/track_model.hpp"
#include "assets/world_database.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// Truncates the speed and wraps to [0, 1), as the original's fixed-point offsets do.
[[nodiscard]] render::UvOffset ScrolledBy(const assets::TextureScroll& scroll, float elapsedMs);

/// `scrolled` indexes `draws`.
void ScrollDraws(std::span<const ScrolledDraws> scrolled, float elapsedMs,
                 std::span<render::MeshDraw> draws);

}
