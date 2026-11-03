#pragma once

#include <cstdint>
#include <span>

#include "assets/image_asset.hpp"
#include "core/vector3.hpp"

namespace rr::app {

inline constexpr std::uint32_t kSilhouettePx = 64;

/// `corners` are car-space triangles. Rows run forward and columns toward the car's left.
[[nodiscard]] assets::ImageAsset ShadowSilhouette(std::span<const Vector3> corners, float length,
                                                  float width);

}
