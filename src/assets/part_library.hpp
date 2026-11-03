#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/geometry_asset.hpp"
#include "core/result.hpp"

namespace rr::assets {

struct PartGroup {
    std::string material;
    std::vector<std::array<std::int32_t, 3>> triangles;
};

struct LibraryPart {
    std::string name;
    float scale = 1.0F;
    std::vector<PartGroup> groups;
};

/// Pool positions are unscaled. Every triangle index is checked against the pool.
struct PartLibrary {
    std::vector<GeometryVertex> vertices;
    std::vector<LibraryPart> parts;
};

[[nodiscard]] Result<PartLibrary> ParsePartLibrary(std::span<const std::byte> bytes);

/// Pools are appended in order. Parts merge by name as `MergeNamed` does.
[[nodiscard]] PartLibrary MergePartLibraries(std::vector<PartLibrary> libraries);

/// Copies only the vertices the part uses, scaled by `scale` rather than the part's own scale.
[[nodiscard]] Result<GeometryAsset> BuildPartGeometry(const PartLibrary& library,
                                                      std::string_view name, float scale);

}
