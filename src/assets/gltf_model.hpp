#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "assets/geometry_asset.hpp"
#include "assets/geometry_writer.hpp"
#include "core/result.hpp"

namespace rr::assets {

struct GltfModel {
    GeometryAsset geometry;
    GdbVertices vertices = GdbVertices::Lit;
};

/// What the replaced game model needs: its skeleton's node names in joint order, and its scale.
struct GltfTarget {
    std::vector<std::string> joints;
    float scale = 1.0F;
};

/// `bytes` is a `.glb` with embedded buffers. Meshes are flattened into game axes (Z up).
[[nodiscard]] Result<GltfModel> ImportGltfModel(std::span<const std::byte> bytes,
                                                const GltfTarget& target = {});

}
