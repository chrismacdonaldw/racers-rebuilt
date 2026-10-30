#pragma once

#include "assets/champion_database.hpp"
#include "assets/chassis_database.hpp"
#include "sim/physics/chassis.hpp"

namespace rr::sim {

/// Takes mass and centre of mass from the champion. Pair it with an empty `CarBuild`.
[[nodiscard]] ChassisSpec PrebuiltChassis(const assets::ChampionEntry& champion,
                                          const assets::ChassisEntry& chassis);

}
