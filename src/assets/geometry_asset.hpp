#pragma once

#include <cstdint>
#include <vector>

#include "core/vector3.hpp"

namespace rr::assets {

struct GeometryVertex {
    Vector3 position;
    Vector3 normal;
    float u = 0.0F;
    float v = 0.0F;
};

/// `indices` is a triangle list into `vertices`.
struct GeometryAsset {
    std::vector<GeometryVertex> vertices;
    std::vector<std::uint32_t> indices;
};

}
