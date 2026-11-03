#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "assets/animation_database.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// The game's own animations give the part order, rates and bounds, which are kept.
struct GltfAnimationTarget {
    std::vector<std::string> joints;
    float scale = 1.0F;
    AnimationAsset original;
};

/// A glTF animation replaces the same-named part. `NotFound` when the file has no animations.
[[nodiscard]] Result<AnimationAsset> ImportGltfAnimations(std::span<const std::byte> bytes,
                                                          const GltfAnimationTarget& target);

}
