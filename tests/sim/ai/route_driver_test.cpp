#include "sim/ai/route_driver.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>

#include "../physics/plane_world.hpp"
#include "straight_route.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr ChassisSpec kNeutralChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
constexpr float kArcRadius = 200.0F;

assets::RaceRoute LeftArc() {
    const float step = std::numbers::pi_v<float> / 180.0F;
    assets::RaceRoute route{.startPosition = {.x = kArcRadius, .z = 0.2F},
                            .startRotation = FromAxisAngle({.z = 1.0F}, step * 90.0F)};
    Vector3 previous = route.startPosition;
    for (std::size_t i = 1; i <= 720; ++i) {
        const float angle = step * static_cast<float>(i);
        const Vector3 next{
            .x = kArcRadius * std::cos(angle), .y = kArcRadius * std::sin(angle), .z = 0.2F};
        route.points.push_back({.offset = next - previous,
                                .rotation = FromAxisAngle({.z = 1.0F}, angle + (step * 90.0F)),
                                .kind = 3,
                                .durationMs = 30});
        previous = next;
    }
    route.loopPosition = route.startPosition + route.points.front().offset;
    route.loopRotation = route.points.front().rotation;
    route.loopTimeMs = route.points.front().durationMs;
    return route;
}

void DriveFor(Racer& racer, RouteDriver& driver, int durationMs, const CollisionQuery& world) {
    for (int elapsed = 0; elapsed < durationMs; elapsed += static_cast<int>(kTick.count())) {
        racer.Tick(kTick, driver.Drive(racer), world);
    }
}

TEST_CASE("a route driver holds a straight route at full throttle") {
    const test::PlaneWorld world = test::FlatGround();
    RouteDriver driver(test::StraightRoute({.z = 0.2F}, 3000));
    Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});

    DriveFor(racer, driver, 8000, world);

    const BodyState& body = racer.Body();
    CHECK(body.motion.position.x > 700.0F);
    CHECK(std::abs(body.motion.position.y) < 0.5F);
    CHECK(body.derived.speed == doctest::Approx(0.12F).epsilon(0.01));
}

TEST_CASE("a paced route driver cruises near its share of the route's recorded speed") {
    const test::PlaneWorld world = test::FlatGround();
    for (const float pace : {1.0F, 3.0F}) {
        CAPTURE(pace);
        RouteDriver driver(test::StraightRoute({.z = 0.2F}, 3000), pace);
        Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});

        DriveFor(racer, driver, 8000, world);

        CHECK(racer.Body().derived.speed == doctest::Approx(0.01F * pace).epsilon(0.15));
    }
}

assets::RaceRoute FastThenSlow() {
    assets::RaceRoute route = test::StraightRoute({.z = 0.2F}, 700);
    for (std::size_t i = 0; i < 400; ++i) {
        route.points[i].durationMs = 10;
    }
    return route;
}

TEST_CASE("a paced route driver brakes ahead of the route slowing down") {
    const test::PlaneWorld world = test::FlatGround();
    RouteDriver driver(FastThenSlow(), 1.0F, {.brakes = true});
    Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});
    float fastest = 0.0F;
    float brakedAtX = 0.0F;
    for (int elapsed = 0; elapsed < 16000; elapsed += static_cast<int>(kTick.count())) {
        const RacerControls controls = driver.Drive(racer);
        if (controls.isBrakeHeld && brakedAtX == 0.0F) {
            brakedAtX = racer.Body().motion.position.x;
        }
        racer.Tick(kTick, controls, world);
        fastest = std::max(fastest, racer.Body().derived.speed);
    }

    CHECK(fastest > 0.05F);
    CHECK(brakedAtX > 300.0F);
    CHECK(brakedAtX < 400.0F);
    CHECK(racer.Body().derived.speed == doctest::Approx(0.01F).epsilon(0.15));
}

TEST_CASE("a paced route driver without braking only coasts ahead of the route slowing down") {
    const test::PlaneWorld world = test::FlatGround();
    RouteDriver driver(FastThenSlow(), 1.0F);
    Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});
    bool hasBraked = false;
    bool hasCoasted = false;
    for (int elapsed = 0; elapsed < 16000; elapsed += static_cast<int>(kTick.count())) {
        const RacerControls controls = driver.Drive(racer);
        hasBraked = hasBraked || controls.isBrakeHeld;
        hasCoasted = hasCoasted || !controls.isThrottleHeld;
        racer.Tick(kTick, controls, world);
    }

    CHECK_FALSE(hasBraked);
    CHECK(hasCoasted);
}

TEST_CASE("a route driver calls a car stalled after 3 s without headway and rejoins it ahead") {
    const test::PlaneWorld world = test::FlatGround();
    RouteDriver driver(test::StraightRoute({.z = 0.2F}, 3000));
    Racer racer(kNeutralChassis, {}, {.position = {.y = 8.0F, .z = 0.2F}});
    int stalledAt = 0;
    for (int elapsed = 12; elapsed <= 4000 && stalledAt == 0; elapsed += 12) {
        static_cast<void>(driver.Drive(racer));
        racer.Tick(kTick, {}, world);
        stalledAt = driver.IsStalled(kTick) ? elapsed : 0;
    }

    const RacerStart rejoin = driver.Rejoin();

    CHECK(stalledAt == doctest::Approx(3000).epsilon(0.05));
    CHECK(rejoin.position.x > 0.0F);
    CHECK(rejoin.position.y == doctest::Approx(0.0F));
    CHECK(rejoin.headingRadians == doctest::Approx(0.0F));
}

void StallFor(RouteDriver& driver, Racer& racer, int durationMs, const CollisionQuery& world) {
    for (int elapsed = 0; elapsed < durationMs; elapsed += static_cast<int>(kTick.count())) {
        static_cast<void>(driver.Drive(racer));
        racer.Tick(kTick, {}, world);
        static_cast<void>(driver.IsStalled(kTick));
    }
}

TEST_CASE("a stuck route driver backs up between 1 s and 1.8 s without headway") {
    const test::PlaneWorld world = test::FlatGround();
    RouteDriver driver(test::StraightRoute({.z = 0.2F}, 3000), std::nullopt, {.unsticks = true});
    Racer racer(kNeutralChassis, {}, {.position = {.y = 8.0F, .z = 0.2F}});

    StallFor(driver, racer, 900, world);
    CHECK_FALSE(driver.Drive(racer).isBrakeHeld);
    StallFor(driver, racer, 300, world);
    const RacerControls backing = driver.Drive(racer);
    StallFor(driver, racer, 800, world);

    CHECK(backing.isBrakeHeld);
    CHECK_FALSE(backing.isThrottleHeld);
    CHECK_FALSE(driver.Drive(racer).isBrakeHeld);
}

TEST_CASE("a stuck route driver without unsticking keeps driving forward") {
    const test::PlaneWorld world = test::FlatGround();
    RouteDriver driver(test::StraightRoute({.z = 0.2F}, 3000));
    Racer racer(kNeutralChassis, {}, {.position = {.y = 8.0F, .z = 0.2F}});

    StallFor(driver, racer, 1200, world);
    const RacerControls controls = driver.Drive(racer);

    CHECK(controls.isThrottleHeld);
    CHECK_FALSE(controls.isBrakeHeld);
}

assets::RaceRoute WideStraightRoute() {
    assets::RaceRoute route = test::StraightRoute({.z = 0.2F}, 3000);
    for (assets::RoutePoint& point : route.points) {
        point.widthLeft = 10.0F;
        point.widthRight = 10.0F;
    }
    return route;
}

TEST_CASE("a route driver steers around a rival just ahead, toward the open side") {
    RouteDriver driver(WideStraightRoute(), std::nullopt, {.passes = true});
    const Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});
    const std::array leftRival{Vector3{.x = 15.0F, .y = 1.0F, .z = 0.2F}};
    const std::array rightRival{Vector3{.x = 15.0F, .y = -1.0F, .z = 0.2F}};
    const std::array farRival{Vector3{.x = 60.0F, .z = 0.2F}};

    CHECK(driver.Drive(racer, leftRival).steerLeftAxis < 0.0F);
    CHECK(driver.Drive(racer, rightRival).steerLeftAxis > 0.0F);
    CHECK(driver.Drive(racer, farRival).steerLeftAxis == 0.0F);
}

TEST_CASE("a route driver doesn't pass its own position in the field") {
    RouteDriver driver(WideStraightRoute(), std::nullopt, {.passes = true});
    const Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});
    const std::array field{Vector3{.x = 60.0F, .z = 0.2F},
                           Vector3{.x = 2.0F, .y = 1.0F, .z = 0.2F}};

    CHECK(driver.Drive(racer, field, 1).steerLeftAxis == 0.0F);
    CHECK(driver.Drive(racer, field, 0).steerLeftAxis < 0.0F);
}

TEST_CASE("a route driver without passing keeps its line behind a rival just ahead") {
    RouteDriver driver(WideStraightRoute());
    const Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});
    const std::array leftRival{Vector3{.x = 15.0F, .y = 1.0F, .z = 0.2F}};

    CHECK(driver.Drive(racer, leftRival).steerLeftAxis == 0.0F);
}

TEST_CASE("an aiming route driver moves to its aim's side of the route instead of passing") {
    RouteDriver driver(WideStraightRoute());
    const Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});
    const std::array leftRival{Vector3{.x = 15.0F, .y = 1.0F, .z = 0.2F}};

    CHECK(driver.Drive(racer, leftRival, {}, Vector3{.x = 80.0F, .y = 5.0F}).steerLeftAxis > 0.0F);
    CHECK(driver.Drive(racer, leftRival, {}, Vector3{.x = 80.0F, .y = -5.0F}).steerLeftAxis < 0.0F);
    CHECK(driver.Drive(racer, leftRival, {}, Vector3{.x = 80.0F}).steerLeftAxis == 0.0F);
}

TEST_CASE("an aiming route driver keeps to its share of the route's width") {
    RouteDriver driver(WideStraightRoute());
    const Racer racer(kNeutralChassis, {}, {.position = {.z = 0.2F}});
    const float edge = driver.Drive(racer, {}, {}, Vector3{.x = 80.0F, .y = 8.0F}).steerLeftAxis;

    CHECK(edge > 0.0F);
    CHECK(driver.Drive(racer, {}, {}, Vector3{.x = 80.0F, .y = 50.0F}).steerLeftAxis == edge);
    CHECK(driver.Drive(racer, {}, {}, Vector3{.x = 80.0F, .y = -50.0F}).steerLeftAxis == -edge);
}

TEST_CASE("a route driver steers back onto a route it starts beside") {
    const test::PlaneWorld world = test::FlatGround();
    RouteDriver driver(test::StraightRoute({.z = 0.2F}, 3000));
    Racer racer(kNeutralChassis, {}, {.position = {.y = 8.0F, .z = 0.2F}});

    DriveFor(racer, driver, 4000, world);

    CHECK(std::abs(racer.Body().motion.position.y) < 1.0F);
}

TEST_CASE("a route driver follows a curve") {
    const test::PlaneWorld world = test::FlatGround();
    RouteDriver driver(LeftArc());
    Racer racer(kNeutralChassis, {},
                {.position = {.x = kArcRadius, .z = 0.2F},
                 .headingRadians = std::numbers::pi_v<float> / 2.0F});

    for (int second = 0; second < 8; ++second) {
        DriveFor(racer, driver, 1000, world);
        const Vector3& position = racer.Body().motion.position;
        CHECK(std::hypot(position.x, position.y) == doctest::Approx(kArcRadius).epsilon(0.03));
    }
}

}
}
