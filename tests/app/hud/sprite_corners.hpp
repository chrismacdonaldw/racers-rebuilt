#pragma once

#include <doctest/doctest.h>

#include <array>

#include "render/render_view.hpp"

namespace rr::test {

inline std::array<render::SpriteCorner, 4> CornersOf(const render::SpriteDraw& sprite) {
    REQUIRE(sprite.corners.has_value());
    return sprite.corners.value_or(std::array<render::SpriteCorner, 4>{});
}

}
