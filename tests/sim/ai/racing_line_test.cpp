#include "sim/ai/racing_line.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "../physics/plane_world.hpp"
#include "bend_route.hpp"

namespace rr::sim {
namespace {

constexpr float kWidth = 10.0F;
constexpr float kRadius = 30.0F;
constexpr float kHalfCar = 2.5F;
constexpr float kProbeSlant = 0.01F;

std::size_t ApexPoint() {
    const std::vector<Vector3> bend = test::LeftBend(kRadius);
    const float eighth = std::numbers::pi_v<float> / 4.0F;
    const Vector3 apex{.x = kRadius * std::sin(eighth), .y = kRadius * (1.0F - std::cos(eighth))};
    const auto nearest = std::ranges::min_element(bend, [&](const Vector3& a, const Vector3& b) {
        return Length(a - apex) < Length(b - apex);
    });
    return static_cast<std::size_t>(nearest - bend.begin()) - 1;
}

RacingLine FitBend(const CollisionQuery& world) {
    return RacingLine::Fit(test::RouteThrough(test::LeftBend(kRadius), kWidth), world);
}

TEST_CASE("a racing line stays half a car inside the route's width") {
    const RacingLine line = FitBend(test::FlatGround());

    REQUIRE_FALSE(line.Laterals().empty());
    const auto [lowest, highest] = std::ranges::minmax(line.Laterals());
    CHECK(lowest >= -(kWidth - kHalfCar) - 1e-4F);
    CHECK(highest <= kWidth - kHalfCar + 1e-4F);
    CHECK(line.WalledPoints() == 0);
}

TEST_CASE("a racing line cuts to the inside of a bend") {
    const RacingLine line = FitBend(test::FlatGround());

    CHECK(line.Laterals()[ApexPoint()] > 2.0F);
}

TEST_CASE("a wall inside the route's width holds the line off it") {
    const float half = std::sqrt(0.5F);
    const Vector3 outward{.x = half, .y = -half};
    const Vector3 centre{.y = kRadius};
    const test::PlaneWorld world(
        {test::TestPlane{},
         test::TestPlane{.normal = outward, .offset = Dot(outward, centre) + kRadius - 3.0F}});
    const RacingLine line = FitBend(world);

    CHECK(line.WalledPoints() > 0);
    CHECK(line.Laterals()[ApexPoint()] <= 3.0F - kHalfCar + kProbeSlant);
}

TEST_CASE("a racing line's lateral blends toward the point being approached") {
    const RacingLine line = FitBend(test::FlatGround());
    const std::size_t apex = ApexPoint();

    CHECK(line.LateralAt(apex, 0.0F) == line.Laterals()[apex - 1]);
    CHECK(line.LateralAt(apex, 1.0F) == line.Laterals()[apex]);
    CHECK(line.LateralAt(line.Laterals().size(), 0.5F) == 0.0F);
    CHECK_FALSE(line.IsClosed());
    CHECK(line.Next(line.Points().size() - 1) == line.Points().size() - 1);
}

}
}
