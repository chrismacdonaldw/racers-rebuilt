#pragma once

#include <array>

#include "core/quaternion.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// x y z w, as files store them.
using StoredRotation = std::array<float, 4>;

inline constexpr StoredRotation kStoredIdentity{0.0F, 0.0F, 0.0F, 1.0F};

/// Files store the rotation that maps row vectors, which is the inverse of the one that maps column
/// vectors. Fails on a zero quaternion.
[[nodiscard]] Result<Quaternion> FromStoredRotation(const StoredRotation& stored);

}
