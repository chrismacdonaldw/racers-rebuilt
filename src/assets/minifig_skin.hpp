#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "core/vector3.hpp"

namespace rr::assets {

struct SkinCorner {
    Vector3 position;
    Vector3 normal;
};

struct SkinnedTriangle {
    std::array<SkinCorner, 3> corners;
    std::array<std::uint16_t, 3> joints{};
};

using JointLocator = std::function<std::uint16_t(const Vector3&)>;

/// Cuts the triangle where its corners follow different joints, so the mesh bends there.
[[nodiscard]] std::vector<SkinnedTriangle> SkinTriangle(const JointLocator& locator,
                                                        const std::array<SkinCorner, 3>& corners);

}
