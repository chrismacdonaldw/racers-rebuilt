#include "sim/ai/route_driver.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <cstddef>
#include <numbers>

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
