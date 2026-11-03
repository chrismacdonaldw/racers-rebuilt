#pragma once

#include <cstddef>
#include <vector>

#include "assets/animation_database.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// Writes what `ParseAnimations` reads. An empty key pool is written as one unused key.
[[nodiscard]] Result<std::vector<std::byte>> WriteAnimations(const AnimationAsset& animations);

}
