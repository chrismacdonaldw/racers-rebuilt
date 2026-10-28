#pragma once

#include "core/quaternion.hpp"
#include "core/vector3.hpp"

namespace rr {

struct Transform {
    Vector3 position;
    Quaternion rotation;

    friend constexpr bool operator==(const Transform&, const Transform&) = default;
};

[[nodiscard]] inline Transform Interpolate(const Transform& from, const Transform& to,
                                           float alpha) {
    return {.position = Lerp(from.position, to.position, alpha),
            .rotation = Nlerp(from.rotation, to.rotation, alpha)};
}

}
