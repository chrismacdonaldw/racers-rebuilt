#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/color.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

struct GeometryVertex {
    Vector3 position;
    Vector3 normal;
    float u = 0.0F;
    float v = 0.0F;
    Rgba8 rgba = kOpaqueWhite;
    std::uint16_t joint = 0;
};

struct GeometryRange {
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t material = 0;

    friend constexpr bool operator==(const GeometryRange&, const GeometryRange&) = default;
};

/// `indices` is a triangle list into `vertices`. Each range draws its slice of `indices` with an
/// entry of `materials`.
struct GeometryAsset {
    std::vector<GeometryVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<std::string> materials;
    std::vector<GeometryRange> ranges;
};

}
