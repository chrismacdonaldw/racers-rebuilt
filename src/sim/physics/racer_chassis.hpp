#pragma once

#include "assets/champion_database.hpp"
#include "assets/chassis_database.hpp"
#include "sim/car_build/built_car.hpp"
#include "sim/physics/chassis.hpp"

namespace rr::sim {

/// Takes mass and centre of mass from the champion. Pair it with an empty `CarBuild`.
[[nodiscard]] ChassisSpec PrebuiltChassis(const assets::ChampionEntry& champion,
                                          const assets::ChassisEntry& chassis);

/// Takes the chassis's own base mass and centre of mass. Pair it with `BuiltCarBuild`.
[[nodiscard]] ChassisSpec BuiltChassis(const assets::ChassisEntry& chassis);

/// Each brick cell adds a unit of mass, and a quarter of the centroid moves the centre of mass.
[[nodiscard]] CarBuild BuiltCarBuild(const BrickVolume& bricks);

}
