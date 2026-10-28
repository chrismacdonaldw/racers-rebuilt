#pragma once

#include "core/duration.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "sim/physics/body_state.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/driver_input.hpp"
#include "sim/physics/racer_controls.hpp"

namespace rr::sim {

struct RacerStart {
    Vector3 position;
    float headingRadians = 0.0F;
};

class Racer {
public:
    Racer(const ChassisSpec& chassis, const CarBuild& build, const RacerStart& start);

    /// `elapsed` is a whole number of ms, at most the original's 40 ms substep.
    void Tick(Milliseconds elapsed, const RacerControls& controls, const CollisionQuery& world);

    /// In game space.
    [[nodiscard]] Transform Pose() const;

    [[nodiscard]] const BodyState& Body() const { return body_; }

    [[nodiscard]] const DriverState& Driver() const { return driver_.State(); }

private:
    void KeepInWorldBounds();

    BodyConstants constants_;
    StatMultipliers stats_;
    BodyState body_;
    DriverInput driver_;
};

}
