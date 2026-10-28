#include "sim/physics/chassis.hpp"

#include <doctest/doctest.h>

namespace rr::sim {
namespace {

TEST_CASE("retail wheel positions give the retail track width and wheelbase") {
    const BodyConstants constants = MakeBodyConstants({}, {});

    CHECK(constants.trackWidth == doctest::Approx(3.978F));
    CHECK(constants.wheelbase == doctest::Approx(2.981F));
    CHECK(constants.wheelAnchor == Vector3{.x = 1.543F, .y = 1.989F, .z = 0.0F});
}

TEST_CASE("high pieces add to the mass and scale it by a millionth") {
    const BodyConstants constants = MakeBodyConstants({}, {.highPieceCount = 40});

    CHECK(constants.mass == 2540.0F);
    CHECK(constants.massScale == doctest::Approx(0.00254F));
}

TEST_CASE("stat multipliers span the spec's ranges") {
    const StatMultipliers lowest =
        MakeStatMultipliers({.handling = 0, .topSpeed = 0, .acceleration = 0}, {});
    const StatMultipliers highest =
        MakeStatMultipliers({.handling = 100, .topSpeed = 100, .acceleration = 100}, {});

    CHECK(lowest.handling == doctest::Approx(0.7F));
    CHECK(lowest.topSpeed == doctest::Approx(0.95F));
    CHECK(highest.handling == doctest::Approx(1.0F));
    CHECK(highest.acceleration == doctest::Approx(1.05F));
}

TEST_CASE("weight trades acceleration for handling and top speed") {
    const ChassisSpec chassis{.handling = 50, .topSpeed = 50, .acceleration = 50};

    const StatMultipliers stats = MakeStatMultipliers(chassis, {.highPieceCount = 100});

    CHECK(stats.handling == doctest::Approx(0.7F + (0.003F * 55.0F)));
    CHECK(stats.topSpeed == doctest::Approx(1.005F));
    CHECK(stats.acceleration == doctest::Approx(0.995F));
}

}
}
