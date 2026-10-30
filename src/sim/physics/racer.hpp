#pragma once

#include <optional>

#include "assets/race_route.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "sim/ai/ai_racer.hpp"
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

/// What happened in the racer's last `Tick`, and in any `Go` or `StartTurbo` call since.
struct RacerEvents {
    std::optional<TurboLevel> turboStarted;
    std::optional<TurboLevel> turboFaded;
    bool hasDriftStarted = false;
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

    /// From now on the racer replays `route` from its start, ignoring controls and the world.
    void FollowRoute(assets::RaceRoute route);

    /// Does nothing unless the racer follows a route.
    void SetRouteSpeed(float speed);

    [[nodiscard]] const std::optional<AiRacer>& Route() const { return route_; }

    /// In game space.
    [[nodiscard]] Transform Pose() const;

    [[nodiscard]] const BodyState& Body() const { return body_; }

    [[nodiscard]] const DriverState& Driver() const { return driver_.State(); }

    [[nodiscard]] const StartBoostState& Boost() const { return startBoost_.State(); }

    [[nodiscard]] const std::optional<Turbo>& ActiveTurbo() const { return turbo_; }

    [[nodiscard]] const RacerEvents& Events() const { return events_; }

    /// `world` is the one this racer stepped against.
    [[nodiscard]] ContactBody ForContacts(const CollisionQuery& world) {
        return {.body = &body_, .constants = &constants_, .world = &world};
    }

private:
    void UpdateTurbo(int dtMs);
    void KeepInWorldBounds();
    void TakeRoutePose(const AiRacer& route);

    BodyConstants constants_;
    StatMultipliers stats_;
    BodyState body_;
    DriverInput driver_;
    StartBoost startBoost_;
    std::optional<Turbo> turbo_;
    std::optional<AiRacer> route_;
    RacerEvents events_;
};

}
