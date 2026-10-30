#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/quaternion.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

/// In the parent's space, in the paired model's unscaled units.
struct SkeletonNode {
    std::string name;
    Vector3 position;
    Quaternion rotation;
    std::optional<std::size_t> parent;
};

/// Node `i` is joint `i` of the paired model. A parent always comes before its children.
struct SkeletonAsset {
    std::vector<SkeletonNode> nodes;
};

[[nodiscard]] Result<SkeletonAsset> ParseSkeleton(std::span<const std::byte> bytes);

}
