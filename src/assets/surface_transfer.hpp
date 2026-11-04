#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "core/vector3.hpp"

namespace rr::assets {

struct SurfaceTriangle {
    std::array<Vector3, 3> corners;
    std::array<float, 3> u{};
    std::array<float, 3> v{};
    Vector3 normal;
    std::uint16_t joint = 0;
    std::uint32_t material = 0;
};

struct SurfacePoint {
    const SurfaceTriangle* triangle = nullptr;
    float distanceSquared = 0.0F;
    float u = 0.0F;
    float v = 0.0F;
};

[[nodiscard]] SurfaceTriangle MakeSurfaceTriangle(const std::array<Vector3, 3>& corners);

/// Triangles facing away from `facing` count `kAwayPenalty` farther. Empty when `surface` is.
[[nodiscard]] SurfacePoint NearestSurfacePoint(std::span<const SurfaceTriangle> surface,
                                               const Vector3& point, const Vector3& facing);

/// A corner whose own UV is over `kSeamGap` from the centre triangle's takes the centre triangle's.
[[nodiscard]] std::array<SurfacePoint, 3> NearestSurfacePoints(
    std::span<const SurfaceTriangle> surface, const std::array<Vector3, 3>& corners,
    const Vector3& facing);

inline constexpr float kAwayPenalty = 0.2F;
inline constexpr float kSeamGap = 0.2F;

}
