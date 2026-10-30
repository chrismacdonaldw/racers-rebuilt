#pragma once

#include "core/transform.hpp"
#include "core/vector3.hpp"

namespace rr::app {

/// Game files are Z-up. The engine is Y-up and looks down -Z.
[[nodiscard]] Vector3 ToEngine(const Vector3& game);

[[nodiscard]] Vector3 ToGame(const Vector3& engine);

/// `forward` and `up` are in game space and need not be unit length or perpendicular.
[[nodiscard]] Transform PlacementToEngine(const Vector3& position, const Vector3& forward,
                                          const Vector3& up);

/// A game-space pose with +X forward, as the engine-space pose of a model that faces game +Y.
[[nodiscard]] Transform CarToEngine(const Transform& game);

[[nodiscard]] Transform TransformToEngine(const Transform& game);

[[nodiscard]] Transform TransformToGame(const Transform& engine);

[[nodiscard]] Transform LookAt(const Vector3& eye, const Vector3& target);

struct Bounds {
    Vector3 min;
    Vector3 max;
    bool isEmpty = true;

    void Add(const Vector3& point);
};

}
