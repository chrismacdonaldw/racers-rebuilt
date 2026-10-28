#include "sim/physics/car_body.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <optional>

#include "plane_world.hpp"
#include "sim/physics/facing.hpp"

namespace rr::sim {
namespace {

constexpr int kTickMs = 12;
constexpr float kThrust = 54.0F;
constexpr float kMaxSpeed = 120.0F;

struct TestCar {
    explicit TestCar(const Vector3& position)
        : constants(MakeBodyConstants({}, {})), body(MakeBody(constants, position, {})) {}

    void PressThrottle() { SetDrive(body, constants, kThrust, kMaxSpeed); }

    void Run(int durationMs, const CollisionQuery& world) {
        for (int elapsed = 0; elapsed < durationMs; elapsed += kTickMs) {
            StepBody(body, constants, kTickMs, world);
        }
    }

    BodyConstants constants;
    BodyState body;
};

TEST_CASE("a car dropped onto flat ground settles on all four wheels at ride height") {
    const test::PlaneWorld world = test::FlatGround();
    TestCar car({.z = 0.2F});

    car.Run(600, world);

    CHECK(car.body.motion.contactCount == 4);
    CHECK(car.body.motion.position.z == doctest::Approx(0.2F));
    CHECK(car.body.derived.speed < 1.0e-4F);
}

TEST_CASE("full thrust matches the spec's 12 ms acceleration figures") {
    const test::PlaneWorld world = test::FlatGround();
    TestCar car({.z = 0.2F});
    car.PressThrottle();
    std::optional<int> ninetyPercentMs;

    for (int elapsedMs = kTickMs; elapsedMs <= 6000; elapsedMs += kTickMs) {
        car.Run(kTickMs, world);
        if (!ninetyPercentMs && car.body.derived.speed >= 0.9F * kMaxSpeed / 1000.0F) {
            ninetyPercentMs = elapsedMs;
        }
    }

    REQUIRE(ninetyPercentMs.has_value());
    CHECK(*ninetyPercentMs == 3264);
    CHECK(car.body.motion.position.x == doctest::Approx(536.8F).epsilon(0.0001));
    CHECK(car.body.derived.speed < kMaxSpeed / 1000.0F);
}

TEST_CASE("grip removes all but 7.8 percent of lateral velocity in 240 ms") {
    const test::PlaneWorld world = test::FlatGround();
    TestCar car({.z = 0.2F});
    car.Run(120, world);
    const float sideways = 0.05F;
    car.body.motion.velocity = {.y = sideways};
    UpdateDerivedMotion(car.body);

    car.Run(240, world);

    CHECK(car.body.motion.velocity.y / sideways == doctest::Approx(0.078F).epsilon(0.02));
}

TEST_CASE("grass under all four wheels cuts the top speed to about 68 units per second") {
    const test::PlaneWorld world({{.surface = {.rollingResistance = 20.0F}}});
    TestCar car({.z = 0.2F});
    car.PressThrottle();

    car.Run(15000, world);

    CHECK(car.body.derived.speed * 1000.0F == doctest::Approx(68.0F).epsilon(0.03));
}

TEST_CASE("a car falling from a height lands and stays on the ground") {
    const test::PlaneWorld world = test::FlatGround();
    TestCar car({.z = 20.0F});

    car.Run(3000, world);

    CHECK(car.body.motion.contactCount == 4);
    CHECK(car.body.motion.position.z == doctest::Approx(0.2F));
    CHECK(car.body.motion.basis.up.z == doctest::Approx(1.0F));
}

TEST_CASE("a wall turns the car aside without letting it through") {
    const test::PlaneWorld world({{}, {.normal = {.x = -1.0F}, .offset = -60.0F}});
    TestCar car({.z = 0.2F});
    car.PressThrottle();
    float furthestX = 0.0F;

    for (int elapsedMs = 0; elapsedMs < 6000; elapsedMs += kTickMs) {
        car.Run(kTickMs, world);
        furthestX = std::max(furthestX, car.body.motion.position.x);
    }

    const Vector3& forward = car.body.motion.basis.forward;
    CHECK(furthestX < 56.0F);
    CHECK(furthestX > 50.0F);
    CHECK(std::abs(std::atan2(forward.y, forward.x)) > 0.5F);
}

TEST_CASE("a non-solid surface lets the car drive through") {
    const test::PlaneWorld world(
        {{}, {.normal = {.x = -1.0F}, .offset = -60.0F, .isSolid = false}});
    TestCar car({.z = 0.2F});
    car.PressThrottle();

    car.Run(3000, world);

    CHECK(car.body.motion.position.x > 100.0F);
}

}
}
