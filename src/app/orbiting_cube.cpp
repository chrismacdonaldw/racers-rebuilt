#include "app/orbiting_cube.hpp"

#include <cmath>

#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr float kOrbitRadius = 2.5F;
constexpr float kOrbitRadiansPerTick = 0.012F;
constexpr float kSpinRadiansPerTick = 0.03F;
constexpr Vector3 kUp{.y = 1.0F};

float Axis(const sim::InputFrame& input, sim::Action negative, sim::Action positive) {
    return (input.IsHeld(positive) ? 1.0F : 0.0F) - (input.IsHeld(negative) ? 1.0F : 0.0F);
}

}

void OrbitingCube::Tick(const sim::InputFrame& input) {
    orbitRadians_ +=
        kOrbitRadiansPerTick * (1.0F + Axis(input, sim::Action::Brake, sim::Action::Accelerate));
    spinRadians_ += kSpinRadiansPerTick *
                    (1.0F + (2.0F * Axis(input, sim::Action::SteerLeft, sim::Action::SteerRight)));
}

Transform OrbitingCube::Pose() const {
    return {.position = {.x = kOrbitRadius * std::cos(orbitRadians_),
                         .y = 0.5F,
                         .z = kOrbitRadius * std::sin(orbitRadians_)},
            .rotation = FromAxisAngle(kUp, spinRadians_)};
}

}
