#include "sim/ai/speed_plan.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "../physics/plane_world.hpp"
#include "bend_route.hpp"
#include "straight_route.hpp"

namespace rr::sim {
namespace {

constexpr CarLimits kCar{
    .topSpeed = 120.0F, .thrust = 54.0F, .brake = 108.0F, .cornerGrip = 39.0F, .lockRadius = 40.0F};

RacingLine LineThrough(const std::vector<Vector3>& positions) {
    return RacingLine::Fit(test::RouteThrough(positions, 0.0F), test::FlatGround());
}

std::vector<Vector3> Circle(float radius) {
    std::vector<Vector3> positions;
    for (int degree = 0; degree <= 360; ++degree) {
        const float angle = std::numbers::pi_v<float> * static_cast<float>(degree) / 180.0F;
        positions.push_back(
            {.x = radius * std::sin(angle), .y = radius * (1.0F - std::cos(angle))});
    }
    return positions;
}

TEST_CASE("a speed plan holds a straight at the car's top speed and never above it") {
    const RacingLine line = RacingLine::Fit(test::StraightRoute({}, 200), test::FlatGround());
    const SpeedPlan plan(line, kCar);

    for (const float speed : plan.Speeds()) {
        CHECK(speed == doctest::Approx(kCar.topSpeed));
    }
    for (const float speed : plan.Reachable()) {
        CHECK(speed <= kCar.topSpeed);
    }
}

TEST_CASE("on a fixed radius the plan drifts at the speed where the drift turn reaches full lock") {
    constexpr float kRadius = 60.0F;
    const RacingLine line = LineThrough(Circle(kRadius));
    const SpeedPlan plan(line, kCar);
    const std::size_t middle = plan.Speeds().size() / 2;

    const float speed = plan.Speeds()[middle];
    const float gripLimit = speed * speed / kCar.cornerGrip;
    CHECK(((4.0F * kRadius) - gripLimit) / 3.0F == doctest::Approx(kCar.lockRadius).epsilon(0.02));
    CHECK(plan.IsDriftAt(middle));
}

TEST_CASE("a speed plan brakes ahead of a tight bend no harder than the car can") {
    const RacingLine line = LineThrough(test::LeftBend(30.0F, 300.0F));
    const SpeedPlan plan(line, kCar);
    const std::span<const float> speeds = plan.Speeds();
    const auto slowest =
        static_cast<std::size_t>(std::ranges::min_element(speeds) - speeds.begin());

    REQUIRE(slowest > 20);
    CHECK(speeds[slowest] < kCar.topSpeed);
    CHECK(speeds[0] == doctest::Approx(kCar.topSpeed));
    for (std::size_t i = 1; i <= slowest; ++i) {
        const float gap = Length(line.Points()[i] - line.Points()[i - 1]);
        const float share = speeds[i] / kCar.topSpeed;
        const float hardest = 2.0F * kCar.brake * (1.0F + (share * share)) * gap;
        CHECK((speeds[i - 1] * speeds[i - 1]) - (speeds[i] * speeds[i]) <= hardest + 1.0F);
    }
}

TEST_CASE("reachable speeds start from standing and never pass the plan") {
    const RacingLine line = LineThrough(test::LeftBend(30.0F, 300.0F));
    const SpeedPlan plan(line, kCar);

    CHECK(plan.Reachable()[0] == 0.0F);
    for (std::size_t i = 0; i < plan.Speeds().size(); ++i) {
        CHECK(plan.Reachable()[i] <= plan.Speeds()[i]);
    }
    CHECK(plan.Reachable()[10] > 0.0F);
    CHECK(plan.LapSeconds() > 0.0F);
}

}
}
