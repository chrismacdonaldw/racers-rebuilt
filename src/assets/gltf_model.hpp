#pragma once

#include <cstddef>
#include <span>

#include "assets/geometry_asset.hpp"
#include "assets/geometry_writer.hpp"
#include "core/result.hpp"

namespace rr::assets {

struct GltfModel {
    GeometryAsset geometry;
    GdbVertices vertices = GdbVertices::Lit;
};

/// `bytes` is a `.glb` with embedded buffers. Meshes are flattened into game axes (Z up).
[[nodiscard]] Result<GltfModel> ImportGltfModel(std::span<const std::byte> bytes);

}
