#include "sim/powerup/aim_cones.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "powerup_rig.hpp"

namespace rr::sim {
namespace {

using test::kChassis;

constexpr Vector3 kForward{.x = 1.0F};
constexpr AimCone kWide{
    .minDistanceSquared = 100.0F, .maxDistanceSquared = 160'000.0F, .cosine = 0.5F};

std::vector<Racer> RacersAt(const std::vector<Vector3>& positions) {
    std::vector<Racer> racers;
    racers.reserve(positions.size());
    for (const Vector3& position : positions) {
        racers.emplace_back(kChassis, CarBuild{}, RacerStart{.position = position});
    }
    return racers;
}

TEST_CASE("the cone searches find the first, nearest and farthest racer ahead") {
    const std::vector<Racer> racers =
        RacersAt({{}, {.x = 300.0F}, {.x = 100.0F}, {.x = 200.0F}, {.x = -50.0F}});

    CHECK(FindRacerInCone(racers, {}, kForward, kWide) == 1);
    CHECK(FindNearestRacerInCone(racers, {}, kForward, kWide) == 2);
    CHECK(FindFarthestRacerInCone(racers, {}, kForward, kWide) == 1);
}

TEST_CASE("the cone leaves out racers too close, too far or off to the side") {
    const std::vector<Racer> racers =
        RacersAt({{.x = 5.0F}, {.x = 500.0F}, {.x = 100.0F, .y = 200.0F}});

    CHECK_FALSE(FindNearestRacerInCone(racers, {}, kForward, kWide));
    CHECK_FALSE(FindFarthestRacerInCone(racers, {}, kForward, kWide));
}

TEST_CASE("the target point search takes the nearest point in the cone") {
    const std::vector<assets::TargetPoint> points{{.position = {.x = 300.0F}},
                                                  {.position = {.x = 150.0F}},
                                                  {.position = {.x = 50.0F, .y = 60.0F}}};

    CHECK(FindNearestTargetPoint(points, {}, kForward, kTargetPointCone) == Vector3{.x = 150.0F});
    CHECK_FALSE(FindNearestTargetPoint(points, {}, -kForward, kTargetPointCone));
}

}
}
