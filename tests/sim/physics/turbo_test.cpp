#include "sim/physics/turbo.hpp"

#include <doctest/doctest.h>

#include <optional>

#include "plane_world.hpp"
#include "sim/physics/racer.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr int kTickMs = 12;
constexpr ChassisSpec kNeutralChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};

int TicksToFinish(TurboLevel level) {
    Turbo turbo{.level = level, .phaseMs = 400};
    int ticks = 1;
    while (AdvanceTurbo(turbo, kTickMs)) {
        ++ticks;
    }
    return ticks;
}

int TicksUntilStalledHoverFades(int boostScale) {
    Turbo turbo{.level = TurboLevel::Hover, .phaseMs = 400, .boostScale = boostScale};
    int ticks = 0;
    while (turbo.phase != TurboPhase::Fade) {
        REQUIRE(AdvanceTurbo(turbo, kTickMs));
        if (CanFadeEarly(turbo)) {
            FadeTurbo(turbo);
        }
        ++ticks;
    }
    return ticks;
}

void Drive(Racer& racer, const RacerControls& controls, int durationMs,
           const CollisionQuery& world) {
    for (int elapsed = 0; elapsed < durationMs; elapsed += kTickMs) {
        racer.Tick(kTick, controls, world);
    }
}

TEST_CASE("each turbo phase lasts whole ticks, dropping the leftover ms") {
    CHECK(TicksToFinish(TurboLevel::Weak) == 34 + 84 + 59);
    CHECK(TicksToFinish(TurboLevel::Strong) == 34 + 125 + 59);
    CHECK(TicksToFinish(TurboLevel::Hover) == 34 + 417 + 59);
}

TEST_CASE("a hover turbo can fade early once 500 ms of its boost have passed") {
    Turbo turbo{.level = TurboLevel::Hover, .phase = TurboPhase::Boost, .phaseMs = 4500};
    CHECK_FALSE(CanFadeEarly(turbo));
    turbo.phaseMs = 4499;
    CHECK(CanFadeEarly(turbo));
    turbo.level = TurboLevel::Strong;
    CHECK_FALSE(CanFadeEarly(turbo));

    turbo.level = TurboLevel::Hover;
    turbo.boostScale = 3;
    turbo.phaseMs = 14500;
    CHECK_FALSE(CanFadeEarly(turbo));
    turbo.phaseMs = 14499;
    CHECK(CanFadeEarly(turbo));

    FadeTurbo(turbo);
    CHECK(turbo.phase == TurboPhase::Fade);
    CHECK(turbo.phaseMs == 700);
}

TEST_CASE("a stalled hover fades 500 ms into its boost however long the boost is") {
    CHECK(TicksUntilStalledHoverFades(1) == 34 + 42);
    CHECK(TicksUntilStalledHoverFades(3) == 34 + 42);
}

TEST_CASE("a hover turbo halts the racer until it ends, and a new turbo lets it go") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});

    racer.StartTurbo(TurboLevel::Hover);
    CHECK(racer.Status().isHalted);
    CHECK(racer.Body().hover.isHovering);

    racer.StartTurbo(TurboLevel::Weak);
    CHECK_FALSE(racer.Status().isHalted);

    racer.StartTurbo(TurboLevel::Hover);
    Drive(racer, {.isThrottleHeld = true}, 509 * kTickMs, world);
    CHECK(racer.Status().isHalted);
    Drive(racer, {.isThrottleHeld = true}, kTickMs, world);
    CHECK_FALSE(racer.ActiveTurbo().has_value());
    CHECK_FALSE(racer.Status().isHalted);
}

TEST_CASE("a turbo raises the top speed and grip until it ends") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});

    racer.StartTurbo(TurboLevel::Weak);

    CHECK(racer.Body().maxSpeed == 176.0F);
    CHECK(racer.Body().gripScale == 1.5F);
    CHECK(racer.Body().impulses.pitchMs == 125);
    Drive(racer, {.isThrottleHeld = true}, 176 * kTickMs, world);
    CHECK(racer.ActiveTurbo().has_value());
    Drive(racer, {.isThrottleHeld = true}, kTickMs, world);
    CHECK_FALSE(racer.ActiveTurbo().has_value());
    CHECK(racer.Body().maxSpeed == 120.0F);
    CHECK(racer.Body().gripScale == 1.0F);
}

TEST_CASE("a turbo pushes the car past its top speed") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});
    Drive(racer, {.isThrottleHeld = true}, 12000, world);

    racer.StartTurbo(TurboLevel::Strong);
    Drive(racer, {}, 2000, world);

    CHECK(racer.Body().impulses.pitchMs == 0);
    CHECK(racer.Body().derived.speed > 0.14F);
}

TEST_CASE("touching a wall halves the turbo thrust for the rest of the turbo") {
    const test::PlaneWorld world({{}, {.normal = {.x = -1.0F}, .offset = -30.0F}});
    Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});

    racer.StartTurbo(TurboLevel::Strong);
    Drive(racer, {}, 1000, world);

    const std::optional<Turbo>& active = racer.ActiveTurbo();
    REQUIRE(active.has_value());
    const Turbo& turbo = *active;
    CHECK(turbo.hasTouchedWall);
    CHECK(racer.Body().thrust == 216.0F);
}

}
}
