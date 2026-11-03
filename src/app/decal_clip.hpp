#pragma once

#include <array>
#include <cstddef>

#include "core/vector3.hpp"

namespace rr::app {

struct DecalVertex {
    Vector3 position;
    float u = 0.0F;
    float v = 0.0F;
};

/// Convex, with room for a triangle cut by all four edges of the decal.
struct DecalPolygon {
    std::array<DecalVertex, 7> corners{};
    std::size_t count = 0;
};

/// Clips to 0 <= u, v <= 1 as the original does: corners cut at 1 take 0.99 there instead.
[[nodiscard]] DecalPolygon ClipToDecal(const std::array<DecalVertex, 3>& triangle);

}
