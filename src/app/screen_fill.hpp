#pragma once

#include "render/render_view.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {

[[nodiscard]] constexpr render::SpriteDraw ScreenFill(render::Color tint) {
    return {.bounds = {.width = render::kUiWidth, .height = render::kUiHeight},
            .tint = tint,
            .anchor = render::SpriteAnchor::Fill};
}

}
