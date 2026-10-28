#pragma once

#include <cmath>

namespace rr {

struct Quaternion {
    float w = 1.0F;
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;

    friend constexpr bool operator==(const Quaternion&, const Quaternion&) = default;
};

[[nodiscard]] constexpr float Dot(const Quaternion& a, const Quaternion& b) {
    return (a.w * b.w) + (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

/// Takes the shorter arc. Both inputs must be unit length.
[[nodiscard]] inline Quaternion Nlerp(const Quaternion& from, const Quaternion& to, float alpha) {
    const float sign = Dot(from, to) < 0.0F ? -1.0F : 1.0F;
    const Quaternion blended{.w = from.w + (((sign * to.w) - from.w) * alpha),
                             .x = from.x + (((sign * to.x) - from.x) * alpha),
                             .y = from.y + (((sign * to.y) - from.y) * alpha),
                             .z = from.z + (((sign * to.z) - from.z) * alpha)};
    const float length = std::sqrt(Dot(blended, blended));
    return {.w = blended.w / length,
            .x = blended.x / length,
            .y = blended.y / length,
            .z = blended.z / length};
}

}
