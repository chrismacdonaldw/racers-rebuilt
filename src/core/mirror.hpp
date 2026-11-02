#pragma once

#include "core/quaternion.hpp"
#include "core/vector3.hpp"

namespace rr {

[[nodiscard]] constexpr Vector3 MirroredY(const Vector3& v) {
    return {.x = v.x, .y = -v.y, .z = v.z};
}

/// The rotation that, between two reflected frames, matches `rotation`.
[[nodiscard]] constexpr Quaternion MirroredY(const Quaternion& rotation) {
    return {.w = -rotation.w, .x = rotation.x, .y = -rotation.y, .z = rotation.z};
}

[[nodiscard]] constexpr Vector3 MirroredYIf(bool isMirrored, const Vector3& v) {
    return isMirrored ? MirroredY(v) : v;
}

}
