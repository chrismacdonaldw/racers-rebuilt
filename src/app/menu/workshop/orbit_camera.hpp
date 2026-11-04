#pragma once

#include "core/bounding_sphere.hpp"
#include "core/duration.hpp"
#include "core/vector3.hpp"

namespace rr::app {

/// In game space. `yaw` and `pitch` are radians, `pitch` up from the ground.
struct OrbitPose {
    Vector3 focus;
    float yaw = 0.0F;
    float pitch = 0.0F;
    float distance = 1.0F;
};

/// Circles a focus point, easing toward where it was last sent.
class OrbitCamera {
public:
    explicit OrbitCamera(OrbitPose home);

    void Turn(float yaw, float pitch);

    /// Drags the scene right and up by fractions of the distance to the focus.
    void Pan(float right, float up);

    /// Positive steps move in.
    void Zoom(float steps);

    /// Keeps the turn, fitting `sphere` in a view `verticalFovDegrees` high.
    void Frame(const BoundingSphere& sphere, float verticalFovDegrees);

    void Home() { goal_ = home_; }

    void Tick(Milliseconds elapsed);

    [[nodiscard]] Vector3 Eye() const;
    [[nodiscard]] Vector3 Focus() const { return shown_.focus; }
    [[nodiscard]] const OrbitPose& Goal() const { return goal_; }

private:
    void Clamp();

    OrbitPose home_;
    OrbitPose goal_;
    OrbitPose shown_;
};

}
