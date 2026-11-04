#pragma once

#include <cmath>

#include "core/vector3.hpp"

namespace rr {

struct Quaternion {
    float w = 1.0F;
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;

    friend constexpr bool operator==(const Quaternion&, const Quaternion&) = default;
};

/// `axis` must be unit length.
[[nodiscard]] inline Quaternion FromAxisAngle(const Vector3& axis, float radians) {
    const float halfSin = std::sin(radians / 2.0F);
    return {.w = std::cos(radians / 2.0F),
            .x = axis.x * halfSin,
            .y = axis.y * halfSin,
            .z = axis.z * halfSin};
}

/// Applies `b` first, then `a`.
[[nodiscard]] constexpr Quaternion operator*(const Quaternion& a, const Quaternion& b) {
    return {.w = (a.w * b.w) - (a.x * b.x) - (a.y * b.y) - (a.z * b.z),
            .x = (a.w * b.x) + (a.x * b.w) + (a.y * b.z) - (a.z * b.y),
            .y = (a.w * b.y) - (a.x * b.z) + (a.y * b.w) + (a.z * b.x),
            .z = (a.w * b.z) + (a.x * b.y) - (a.y * b.x) + (a.z * b.w)};
}

[[nodiscard]] constexpr float Dot(const Quaternion& a, const Quaternion& b) {
    return (a.w * b.w) + (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

[[nodiscard]] constexpr Quaternion Conjugate(const Quaternion& q) {
    return {.w = q.w, .x = -q.x, .y = -q.y, .z = -q.z};
}

/// No turn when `from` and `to` are parallel or opposite.
[[nodiscard]] inline Quaternion RotationBetween(const Vector3& from, const Vector3& to) {
    constexpr float kParallel = 1e-6F;
    const Vector3 a = Normalized(from, {});
    const Vector3 b = Normalized(to, {});
    const Vector3 axis = Cross(a, b);
    const float sine = Length(axis);
    if (sine < kParallel) {
        return {};
    }
    return FromAxisAngle(axis * (1.0F / sine), std::atan2(sine, Dot(a, b)));
}

/// `rotation` must be unit length.
[[nodiscard]] constexpr Vector3 Rotate(const Quaternion& rotation, const Vector3& v) {
    const Vector3 axis{.x = rotation.x, .y = rotation.y, .z = rotation.z};
    const Vector3 twice = 2.0F * Cross(axis, v);
    return v + (rotation.w * twice) + Cross(axis, twice);
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
