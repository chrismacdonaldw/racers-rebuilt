#pragma once

#include "render/render_view.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {

/// Reaches past the 4:3 canvas so it also covers the side bars of a wider window.
[[nodiscard]] constexpr render::SpriteDraw ScreenFill(render::Color tint) {
    constexpr float kSpan = 3.0F;
    return {.bounds = {.x = -render::kUiWidth,
                       .y = -render::kUiHeight,
                       .width = kSpan * render::kUiWidth,
                       .height = kSpan * render::kUiHeight},
            .tint = tint};
}

}
