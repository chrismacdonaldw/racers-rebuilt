#pragma once

#include "core/quaternion.hpp"
#include "core/vector3.hpp"

namespace rr {

struct Transform {
    Vector3 position;
    Quaternion rotation;

    friend constexpr bool operator==(const Transform&, const Transform&) = default;
};

[[nodiscard]] constexpr Vector3 Apply(const Transform& transform, const Vector3& point) {
    return transform.position + Rotate(transform.rotation, point);
}

/// Applies `child` first, then `parent`.
[[nodiscard]] constexpr Transform Compose(const Transform& parent, const Transform& child) {
    return {.position = Apply(parent, child.position),
            .rotation = parent.rotation * child.rotation};
}

[[nodiscard]] constexpr Transform Inverse(const Transform& transform) {
    const Quaternion undo = Conjugate(transform.rotation);
    return {.position = Rotate(undo, -transform.position), .rotation = undo};
}

[[nodiscard]] inline Transform Interpolate(const Transform& from, const Transform& to,
                                           float alpha) {
    return {.position = Lerp(from.position, to.position, alpha),
            .rotation = Nlerp(from.rotation, to.rotation, alpha)};
}

}
