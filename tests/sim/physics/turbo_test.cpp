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

void Drive(Racer& racer, const RacerControls& controls, int durationMs,
           const CollisionQuery& world) {
    for (int elapsed = 0; elapsed < durationMs; elapsed += kTickMs) {
        racer.Tick(kTick, controls, world);
    }
}

TEST_CASE("each turbo phase lasts whole ticks, dropping the leftover ms") {
    CHECK(TicksToFinish(TurboLevel::Weak) == 34 + 84 + 59);
    CHECK(TicksToFinish(TurboLevel::Strong) == 34 + 125 + 59);
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
