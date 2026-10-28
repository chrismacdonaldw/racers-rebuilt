#pragma once

namespace rr {

struct Vector3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;

    friend constexpr bool operator==(const Vector3&, const Vector3&) = default;
};

[[nodiscard]] constexpr Vector3 Lerp(const Vector3& from, const Vector3& to, float alpha) {
    return {.x = from.x + ((to.x - from.x) * alpha),
            .y = from.y + ((to.y - from.y) * alpha),
            .z = from.z + ((to.z - from.z) * alpha)};
}

}
