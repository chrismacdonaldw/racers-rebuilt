#pragma once

#include <array>

#include "core/quaternion.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// x y z w, as files store them.
using StoredRotation = std::array<float, 4>;

inline constexpr StoredRotation kStoredIdentity{0.0F, 0.0F, 0.0F, 1.0F};

/// Inverts the stored row-vector rotation. Fails on a zero quaternion.
[[nodiscard]] Result<Quaternion> FromStoredRotation(const StoredRotation& stored);

}
