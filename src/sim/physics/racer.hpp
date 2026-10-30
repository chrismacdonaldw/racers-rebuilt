#pragma once

#include <optional>

#include "core/duration.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "sim/physics/body_state.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/driver_input.hpp"
#include "sim/physics/racer_contacts.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/physics/start_boost.hpp"
#include "sim/physics/turbo.hpp"

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

    /// Pedals and drift do nothing until `Go`.
    void HoldAtStart();
    void Go();

    /// Replaces any turbo already running.
    void StartTurbo(TurboLevel level);

    /// Puts the racer at rest at `start`, as if it were built there.
    void Place(const RacerStart& start);

    /// In game space.
    [[nodiscard]] Transform Pose() const;

    [[nodiscard]] const BodyState& Body() const { return body_; }

    [[nodiscard]] const DriverState& Driver() const { return driver_.State(); }

    [[nodiscard]] const StartBoostState& Boost() const { return startBoost_.State(); }

    [[nodiscard]] const std::optional<Turbo>& ActiveTurbo() const { return turbo_; }

    /// `world` is the one this racer stepped against.
    [[nodiscard]] ContactBody ForContacts(const CollisionQuery& world) {
        return {.body = &body_, .constants = &constants_, .world = &world};
    }

private:
    void UpdateTurbo(int dtMs);
    void KeepInWorldBounds();

    BodyConstants constants_;
    StatMultipliers stats_;
    BodyState body_;
    DriverInput driver_;
    StartBoost startBoost_;
    std::optional<Turbo> turbo_;
};

}
