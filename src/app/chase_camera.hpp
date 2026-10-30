#pragma once

#include "core/transform.hpp"
#include "core/vector3.hpp"

namespace rr::app {

/// Trails behind and above a car. Cars are engine-space poses of models that face game +Y.
class ChaseCamera {
public:
    explicit ChaseCamera(const Transform& car);

    /// Eases toward the spot behind `car`. Call once per tick.
    void Follow(const Transform& car);

    void JumpTo(const Transform& car);

    [[nodiscard]] Transform Pose(const Transform& car) const;

private:
    Vector3 position_;
};

}
