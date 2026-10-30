#pragma once

#include "core/transform.hpp"
#include "core/vector3.hpp"

namespace rr::app {

/// Game files are Z-up. The engine is Y-up and looks down -Z.
[[nodiscard]] Vector3 ToEngine(const Vector3& game);

/// `forward` and `up` are in game space and need not be unit length or perpendicular.
[[nodiscard]] Transform PlacementToEngine(const Vector3& position, const Vector3& forward,
                                          const Vector3& up);

[[nodiscard]] Transform LookAt(const Vector3& eye, const Vector3& target);

struct Bounds {
    Vector3 min;
    Vector3 max;
    bool isEmpty = true;

    void Add(const Vector3& point);
};

}
