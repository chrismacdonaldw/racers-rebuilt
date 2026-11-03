#pragma once

#include <vector>

#include "app/decal_surface.hpp"
#include "app/track_space.hpp"
#include "core/vector3.hpp"

namespace rr::test {

inline constexpr double kClipEpsilon = 0.002;

[[nodiscard]] inline app::DecalTriangle GameTriangle(const Vector3& a, const Vector3& b,
                                                     const Vector3& c) {
    return {app::ToEngine(a), app::ToEngine(b), app::ToEngine(c)};
}

[[nodiscard]] inline app::DecalSurface FlatDecalGround(float height = 0.0F) {
    return app::DecalSurface(std::vector{GameTriangle({.x = -100.0F, .y = -100.0F, .z = height},
                                                      {.x = 300.0F, .y = -100.0F, .z = height},
                                                      {.x = -100.0F, .y = 300.0F, .z = height})});
}

}
