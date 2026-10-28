#include "sim/physics/racer.hpp"

#include <doctest/doctest.h>

#include <cmath>

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

}
}
