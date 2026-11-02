#pragma once

#include <algorithm>
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
    std::uint16_t joint = 0;

    friend constexpr bool operator==(const GeometryRange&, const GeometryRange&) = default;
};

/// Ranges follow each triangle's first-corner joint. Positions are already multiplied by `scale`.
struct GeometryAsset {
    std::vector<GeometryVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<std::string> materials;
    std::vector<GeometryRange> ranges;
    float scale = 1.0F;
};

[[nodiscard]] inline std::uint32_t FindOrAddMaterial(std::vector<std::string>& materials,
                                                     const std::string& material) {
    const auto found = std::ranges::find(materials, material);
    if (found == materials.end()) {
        materials.push_back(material);
        return static_cast<std::uint32_t>(materials.size() - 1);
    }
    return static_cast<std::uint32_t>(found - materials.begin());
}

}
