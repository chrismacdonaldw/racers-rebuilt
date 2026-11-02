#pragma once

#include <cstddef>

#include "render/render_view.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {

/// In UI units. Rows measure their y from `top`.
struct HudPlacement {
    float top = 0.0F;
    float bottom = render::kUiHeight;
    float textScale = 1.0F;
    float bannerScale = 1.0F;
    float mapScale = 1.0F;
    bool isSplit = false;
};

/// Full screen for one player. With two, player 1 takes the top half and player 2 the bottom.
[[nodiscard]] HudPlacement PlacementFor(std::size_t player, std::size_t players);

[[nodiscard]] render::Rect ViewportOf(const HudPlacement& placement);

}
