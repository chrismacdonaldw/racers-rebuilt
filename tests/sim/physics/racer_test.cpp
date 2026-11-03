#include "sim/physics/racer.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <optional>

#include "../ai/straight_route.hpp"
#include "plane_world.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr ChassisSpec kNeutralChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};

void Drive(Racer& racer, const RacerControls& controls, int durationMs,
           const CollisionQuery& world) {
    for (int elapsed = 0; elapsed < durationMs; elapsed += static_cast<int>(kTick.count())) {
        racer.Tick(kTick, controls, world);
    }
}

float Heading(const Racer& racer) {
    const Vector3& forward = racer.Body().motion.basis.forward;
    return std::atan2(forward.y, forward.x);
}

TEST_CASE("held throttle reaches the chassis top speed") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});

    Drive(racer, {.isThrottleHeld = true}, 12000, world);

    CHECK(racer.Body().derived.speed == doctest::Approx(0.12F).epsilon(0.001));
}

TEST_CASE("a body passing a finish surface reaches the finish for that tick") {
    constexpr float kFinishX = 10.0F;
    const test::PlaneWorld world({test::TestPlane{}, test::TestPlane{.normal = {.x = -1.0F},
                                                                     .offset = -kFinishX,
                                                                     .surface = {.isFinish = true},
                                                                     .isSolid = false}});
    Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});
    int finishes = 0;
    for (int tick = 0; tick < 1000 && racer.Body().motion.position.x < kFinishX * 2.0F; ++tick) {
        racer.Tick(kTick, {.isThrottleHeld = true}, world);
        finishes += racer.Events().touches.HasReachedFinish() ? 1 : 0;
    }

    CHECK(racer.Body().motion.position.x >= kFinishX * 2.0F);
    CHECK(finishes > 0);
    CHECK_FALSE(racer.Events().touches.HasReachedFinish());
}

TEST_CASE("holding left turns the car counter-clockwise") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});
    Drive(racer, {.isThrottleHeld = true}, 1500, world);

    Drive(racer, {.isLeftHeld = true, .isThrottleHeld = true}, 1000, world);

    CHECK(Heading(racer) > 0.3F);
    CHECK(racer.Body().motion.position.y > 0.0F);
    CHECK(racer.Body().motion.contactCount == 4);
}

TEST_CASE("braking from speed stops the car and then reverses it") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});
    Drive(racer, {.isThrottleHeld = true}, 2000, world);

    Drive(racer, {.isBrakeHeld = true}, 6000, world);

    CHECK(racer.Body().derived.forwardSpeed < 0.0F);
}

TEST_CASE("a drift and a turbo each report their start once") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});
    Drive(racer, {.isThrottleHeld = true}, 1500, world);
    int driftStarts = 0;

    for (int tick = 0; tick < 80; ++tick) {
        racer.Tick(kTick, {.isLeftHeld = true, .isThrottleHeld = true, .isDriftHeld = true}, world);
        driftStarts += racer.Events().hasDriftStarted ? 1 : 0;
    }
    racer.StartTurbo(TurboLevel::Weak);

    CHECK(driftStarts == 1);
    CHECK(racer.Events().turboStarted == TurboLevel::Weak);
    racer.Tick(kTick, {.isThrottleHeld = true}, world);
    CHECK_FALSE(racer.Events().turboStarted.has_value());
}

TEST_CASE("the racer starts facing its heading") {
    const Racer racer(kNeutralChassis, {}, {.headingRadians = 1.0F});

    CHECK(Heading(racer) == doctest::Approx(1.0F));
}

TEST_CASE("leaving the world bounds restores the car and stops it") {
    const test::PlaneWorld world({});
    Racer racer(kNeutralChassis, {}, {.position = {.z = -240.0F}});

    Drive(racer, {}, 3000, world);

    CHECK(racer.Body().motion.position.z >= -250.0F);
    CHECK(racer.Body().motion.velocity.z > -0.2F);
}

TEST_CASE("a racer on a route starts where it starts and ignores controls and the world") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer(kNeutralChassis, {}, {.position = {.x = 5.0F, .z = 0.2F}});

    racer.FollowRoute(test::StraightRoute({.x = -20.0F, .z = 0.2F}));
    CHECK(racer.Body().motion.position == Vector3{.x = -20.0F, .z = 0.2F});

    Drive(racer, {.isLeftHeld = true, .isBrakeHeld = true}, 1200, world);

    CHECK(racer.Body().motion.position.x == doctest::Approx(-8.0F));
    CHECK(racer.Body().derived.forwardSpeed == doctest::Approx(0.01F));
    CHECK(Heading(racer) == doctest::Approx(0.0F));
    CHECK(racer.Body().motion.contactCount == 4);
}

TEST_CASE("a racer on a route waits at its start until go") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer(kNeutralChassis, {}, {});
    racer.FollowRoute(test::StraightRoute({}));
    racer.HoldAtStart();

    Drive(racer, {}, 600, world);
    CHECK(racer.Body().motion.position.x == 0.0F);

    racer.Go();
    Drive(racer, {}, 600, world);
    CHECK(racer.Body().motion.position.x == doctest::Approx(6.0F));
}

test::PlaneWorld WallAcross(float x, bool isSolid) {
    return test::PlaneWorld({test::TestPlane{
        .normal = {.x = -1.0F}, .offset = -x, .surface = {.touchEvent = 101}, .isSolid = isSolid}});
}

TEST_CASE("a racer on a route touches the faces it drives through") {
    const test::PlaneWorld world = WallAcross(-15.0F, false);
    Racer racer(kNeutralChassis, {}, {});
    racer.FollowRoute(test::StraightRoute({.x = -20.0F, .z = 0.2F}));
    int touches = 0;

    for (int tick = 0; tick < 100; ++tick) {
        racer.Tick(kTick, {}, world);
        touches += static_cast<int>(racer.Events().touches.Touches().size());
    }

    CHECK(touches > 0);
    CHECK(racer.Body().motion.position.x > -15.0F);
}

TEST_CASE("a racer on a route stops at a solid face and backs off") {
    const test::PlaneWorld world = WallAcross(-15.0F, true);
    Racer racer(kNeutralChassis, {}, {});
    racer.FollowRoute(test::StraightRoute({.x = -20.0F, .z = 0.2F}));
    float slowest = 1.0F;

    for (int tick = 0; tick < 200; ++tick) {
        racer.Tick(kTick, {}, world);
        if (const std::optional<AiRacer>& route = racer.Route()) {
            slowest = std::min(slowest, route->PlaybackSpeed());
        }
    }

    CHECK(racer.Body().motion.position.x < -15.0F);
    CHECK(slowest == doctest::Approx(-0.1F));
}

TEST_CASE("only a racer on a route takes a route speed") {
    const test::PlaneWorld world = test::FlatGround();
    Racer driven(kNeutralChassis, {}, {.position = {.z = 0.2F}});
    Racer routed(kNeutralChassis, {}, {});
    routed.FollowRoute(test::StraightRoute({}));

    driven.SetRouteSpeed(1.5F);
    routed.SetRouteSpeed(1.5F);
    Drive(routed, {}, 1200, world);

    const auto speed = [](const AiRacer& route) { return route.PlaybackSpeed(); };
    CHECK_FALSE(driven.Route().has_value());
    CHECK(routed.Route().transform(speed) == 1.5F);
}

}
}
