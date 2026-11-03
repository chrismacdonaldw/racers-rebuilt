#include <doctest/doctest.h>

#include <cmath>
#include <numbers>

#include "plane_world.hpp"
#include "sim/physics/car_body.hpp"
#include "sim/physics/racer.hpp"
#include "sim/race/random.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr int kTickMs = 12;
constexpr ChassisSpec kNeutralChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};

Racer RacerOnGround() {
    return {kNeutralChassis, {}, {.position = {.z = 0.2F}}};
}

void Drive(Racer& racer, const RacerControls& controls, int durationMs,
           const CollisionQuery& world) {
    for (int elapsed = 0; elapsed < durationMs; elapsed += kTickMs) {
        racer.Tick(kTick, controls, world);
        racer.UpdateTimers(kTick);
    }
}

void DriveWatchingForStops(Racer& racer, int durationMs, const CollisionQuery& world) {
    for (int elapsed = 0; elapsed < durationMs; elapsed += kTickMs) {
        Drive(racer, {.isThrottleHeld = true}, kTickMs, world);
        racer.WatchForSuddenStop();
    }
}

float Heading(const Racer& racer) {
    const Vector3& forward = racer.Body().motion.basis.forward;
    return std::atan2(forward.y, forward.x);
}

TEST_CASE("a spinning car turns 2.012 turns and keeps its spin until step 152") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer = RacerOnGround();
    Drive(racer, {}, 120, world);

    racer.StartSpin(2.0F, 0.007F);
    float turned = 0.0F;
    float heading = Heading(racer);
    for (int step = 1; step <= 152; ++step) {
        CHECK(racer.IsSpinning());
        Drive(racer, {}, kTickMs, world);
        const float now = Heading(racer);
        turned += std::remainder(now - heading, 2.0F * std::numbers::pi_v<float>);
        heading = now;
    }

    CHECK_FALSE(racer.IsSpinning());
    CHECK(turned == doctest::Approx(12.642F).epsilon(0.01));
}

TEST_CASE("braking ends an aligned spin within the turn") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer = RacerOnGround();
    racer.StartSpin(1.0F, 0.007F, 1.0F);
    Drive(racer, {}, 120, world);

    Drive(racer, {.isBrakeHeld = true}, kTickMs, world);

    CHECK(racer.Body().impulses.yawMs == 897 - 132);
}

TEST_CASE("a player's spin-out ends with the tick it starts in") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer = RacerOnGround();

    racer.StartSpinOut();
    racer.Tick(kTick, {}, world);
    CHECK(racer.Body().isSpunOut);
    racer.UpdateTimers(kTick);

    CHECK_FALSE(racer.Body().isSpunOut);
}

TEST_CASE("a launch hit replaces the car's velocity with a forward and upward throw") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer = RacerOnGround();
    Drive(racer, {.isThrottleHeld = true}, 1000, world);
    const float mass = MakeBodyConstants(kNeutralChassis, {}).mass;
    const Vector3 facing = racer.Body().facing;

    racer.LaunchHit(1.0F);

    const Vector3& velocity = racer.Body().motion.velocity;
    CHECK(velocity.x == doctest::Approx(facing.x * 80.0F / mass));
    CHECK(velocity.z == doctest::Approx(((facing.z * 80.0F) + 200.0F) / mass));
    CHECK(racer.Body().isSpunOut);
}

TEST_CASE("a halted car lifts to hover 6 units up at 3 a second and drops back on resume") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer = RacerOnGround();
    Drive(racer, {}, 240, world);

    racer.Halt();
    Drive(racer, {}, 996, world);
    CHECK(racer.Body().hover.lift.height == doctest::Approx(2.988F));
    Drive(racer, {}, 1500, world);

    CHECK(racer.Body().hover.lift.height == doctest::Approx(6.0F));
    CHECK(racer.Body().hover.frontAxle.z == doctest::Approx(6.0F).epsilon(0.01));
    CHECK(racer.Body().motion.contactCount == 2);
    racer.Resume();
    Drive(racer, {}, 3000, world);
    CHECK_FALSE(racer.Body().hover.isHovering);
    CHECK(racer.Body().motion.contactCount == 4);
    CHECK(racer.Body().motion.position.z < 1.0F);
}

TEST_CASE("a curse reverses steering, then lifts on the tick its 10 s run out") {
    const test::PlaneWorld world = test::FlatGround();
    Racer cursed = RacerOnGround();
    Racer clean = RacerOnGround();
    const RacerControls left{.isLeftHeld = true, .isThrottleHeld = true};
    Random random(7);

    cursed.AttachCurse(Milliseconds{10'000});
    int ticks = 0;
    while (cursed.Status().isCursed) {
        cursed.RollCurse(kTick, random);
        cursed.Tick(kTick, left, world);
        cursed.UpdateTimers(kTick);
        if (++ticks == 60) {
            Drive(clean, left, 60 * kTickMs, world);
            CHECK(cursed.Body().turnRadius * clean.Body().turnRadius < 0.0F);
        }
    }

    CHECK(ticks == 834);
    CHECK(cursed.Status().hitReactions == 1);
}

TEST_CASE("a cursed racer can't raise its top speed with a turbo") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer = RacerOnGround();
    racer.AttachCurse(Milliseconds{10'000});

    racer.StartTurbo(TurboLevel::Strong);

    CHECK(racer.ActiveTurbo().has_value());
    CHECK(racer.Body().maxSpeed == doctest::Approx(kBaseMaxSpeed * racer.Stats().topSpeed));
    CHECK(racer.Body().gripScale == 1.0F);
}

TEST_CASE("an external force pushes the car every step until it ends") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer = RacerOnGround();

    racer.StartExternalForce(1, {.y = 180.0F});
    Drive(racer, {}, 600, world);
    const float pushed = racer.Body().motion.velocity.y;
    racer.EndExternalForce(1);
    Drive(racer, {}, 600, world);

    CHECK(pushed > 0.01F);
    CHECK(racer.Body().motion.velocity.y < pushed);
}

TEST_CASE("a magnet hold brakes a player to a stop whatever the pedals do") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer = RacerOnGround();
    Drive(racer, {.isThrottleHeld = true}, 2000, world);

    racer.StartMagnetHold();
    Drive(racer, {.isThrottleHeld = true}, 3000, world);
    CHECK(std::abs(racer.Body().derived.forwardSpeed) < 0.001F);
    racer.EndMagnetHold();
    Drive(racer, {.isThrottleHeld = true}, 1000, world);

    CHECK(racer.Body().derived.forwardSpeed > 0.02F);
}

TEST_CASE("a ghost drives through track walls and is stopped again once it leaves") {
    const test::PlaneWorld world(
        {test::TestPlane{}, test::TestPlane{.normal = {.x = -1.0F}, .offset = -20.0F}});
    Racer ghost = RacerOnGround();
    Racer solid = RacerOnGround();

    ghost.EnterGhostMode();
    Drive(ghost, {.isThrottleHeld = true}, 3000, world);
    Drive(solid, {.isThrottleHeld = true}, 3000, world);

    CHECK(ghost.Body().motion.position.x > 30.0F);
    CHECK(solid.Body().motion.position.x < 20.0F);
    ghost.LeaveGhostMode();
    CHECK(ghost.Body().hasTrackCollision);
}

TEST_CASE("respawning clears every effect but keeps the hit count") {
    Racer racer = RacerOnGround();
    racer.AttachCurse(Milliseconds{10'000});
    racer.Halt();
    racer.StartSpin(1.0F, 0.01F);

    racer.Place({.position = {.z = 0.2F}});

    CHECK_FALSE(racer.Status().isCursed);
    CHECK_FALSE(racer.Status().isHalted);
    CHECK_FALSE(racer.IsSpinning());
    CHECK(racer.Status().hitReactions == 1);
}

TEST_CASE("a sudden stop counts once and the count survives a respawn") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer = RacerOnGround();
    DriveWatchingForStops(racer, 1000, world);
    CHECK(racer.Status().suddenStops == 0);

    racer.Place({.position = {.z = 0.2F}});
    racer.WatchForSuddenStop();
    racer.WatchForSuddenStop();
    CHECK(racer.Status().suddenStops == 1);

    racer.Place({.position = {.z = 0.2F}});
    CHECK(racer.Status().suddenStops == 1);
}

}
}
