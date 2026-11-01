#pragma once

#include <optional>
#include <span>
#include <vector>

#include "core/quaternion.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// Chases a point circling above a cursed car, in game space.
class CurseOrbit {
public:
    explicit CurseOrbit(const Vector3& from) : position_(from) {}

    /// `speed` is the car's, in units per ms.
    void Follow(const Vector3& car, float speed, float elapsedMs);

    [[nodiscard]] Transform Pose() const { return {.position = position_, .rotation = facing_}; }
    [[nodiscard]] float Scale() const { return scale_; }

private:
    Vector3 position_;
    Quaternion facing_;
    float phaseMs_ = 0.0F;
    float scale_ = 1.0F;
};

/// Starts each newly cursed car's curse at the curse that caught it.
class CursedCars {
public:
    void Update(const sim::RenderSnapshot& snapshot, float elapsedMs);

    /// Indexed by racer.
    [[nodiscard]] std::span<const std::optional<CurseOrbit>> Orbits() const { return orbits_; }

private:
    std::vector<std::optional<CurseOrbit>> orbits_;
};

}
