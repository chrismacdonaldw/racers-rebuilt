#pragma once

#include <cstddef>
#include <vector>

#include "assets/animation_database.hpp"
#include "assets/geometry_asset.hpp"
#include "assets/skeleton_database.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// `animations` is ignored without `skeleton`.
struct GltfSource {
    const GeometryAsset* geometry = nullptr;
    const SkeletonAsset* skeleton = nullptr;
    const AnimationAsset* animations = nullptr;
};

/// With a skeleton, the mesh is skinned in its bind pose and each animation part is an animation.
[[nodiscard]] Result<std::vector<std::byte>> ExportGltf(const GltfSource& source);

}
