#pragma once

#include <cmath>

namespace rr {

struct Vector3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;

    friend constexpr bool operator==(const Vector3&, const Vector3&) = default;
};

[[nodiscard]] constexpr Vector3 operator+(const Vector3& a, const Vector3& b) {
    return {.x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z};
}

[[nodiscard]] constexpr Vector3 operator-(const Vector3& a, const Vector3& b) {
    return {.x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z};
}

[[nodiscard]] constexpr Vector3 operator-(const Vector3& v) {
    return {.x = -v.x, .y = -v.y, .z = -v.z};
}

[[nodiscard]] constexpr Vector3 operator*(const Vector3& v, float scale) {
    return {.x = v.x * scale, .y = v.y * scale, .z = v.z * scale};
}

[[nodiscard]] constexpr Vector3 operator*(float scale, const Vector3& v) {
    return v * scale;
}

constexpr Vector3& operator+=(Vector3& a, const Vector3& b) {
    a = a + b;
    return a;
}

constexpr Vector3& operator-=(Vector3& a, const Vector3& b) {
    a = a - b;
    return a;
}

[[nodiscard]] constexpr float Dot(const Vector3& a, const Vector3& b) {
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

[[nodiscard]] constexpr Vector3 Cross(const Vector3& a, const Vector3& b) {
    return {.x = (a.y * b.z) - (a.z * b.y),
            .y = (a.z * b.x) - (a.x * b.z),
            .z = (a.x * b.y) - (a.y * b.x)};
}

[[nodiscard]] inline float Length(const Vector3& v) {
    return std::sqrt(Dot(v, v));
}

/// Returns `fallback` for a zero vector.
[[nodiscard]] inline Vector3 Normalized(const Vector3& v, const Vector3& fallback) {
    const float length = Length(v);
    return length > 0.0F ? v * (1.0F / length) : fallback;
}

/// `axis` is unit length.
[[nodiscard]] inline Vector3 RotateAbout(const Vector3& v, const Vector3& axis, float radians) {
    const float cos = std::cos(radians);
    return (v * cos) + (Cross(axis, v) * std::sin(radians)) +
           (axis * (Dot(axis, v) * (1.0F - cos)));
}

[[nodiscard]] constexpr Vector3 Lerp(const Vector3& from, const Vector3& to, float alpha) {
    return {.x = from.x + ((to.x - from.x) * alpha),
            .y = from.y + ((to.y - from.y) * alpha),
            .z = from.z + ((to.z - from.z) * alpha)};
}

}
