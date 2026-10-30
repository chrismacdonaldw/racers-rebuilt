#include "sim/physics/prebuilt_chassis.hpp"

#include <doctest/doctest.h>

namespace rr::sim {
namespace {

TEST_CASE("a prebuilt racer weighs what its champion does") {
    const assets::ChampionEntry champion{.mass = 4500.0F, .centreOfMass = {.x = -1.0F, .z = -1.0F}};
    assets::ChassisEntry chassis{.builtCentreOfMass = {.x = -1.0F, .z = -0.5F},
                                 .baseMass = 2500.0F,
                                 .shadowWidth = 8.0F,
                                 .shadowLength = 9.0F,
                                 .handling = 70,
                                 .topSpeed = 60,
                                 .acceleration = 100};
    chassis.wheelPositions[0] = {.x = 1.5F, .y = -2.0F};

    const ChassisSpec spec = PrebuiltChassis(champion, chassis);
    const BodyConstants constants = MakeBodyConstants(spec, CarBuild{});

    CHECK(constants.mass == 4500.0F);
    CHECK(constants.centreOfMass == Vector3{.x = -1.0F, .z = -1.0F});
    CHECK(spec.shadowLength == 9.0F);
    CHECK(spec.wheelPositions[0] == Vector3{.x = 1.5F, .y = -2.0F});
    CHECK(spec.handling == 70);
    CHECK(spec.topSpeed == 60);
    CHECK(spec.acceleration == 100);
}

}
}
