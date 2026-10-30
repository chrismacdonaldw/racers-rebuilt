#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sim/physics/chassis.hpp"
#include "sim/race/respawn.hpp"

namespace rr::sim {

struct RacerEntry {
    ChassisSpec chassis;
    CarBuild build;
    bool isPlayer = false;
};

/// Racer `i` starts in grid slot `i`.
struct RaceSetup {
    std::string track;
    std::vector<RacerEntry> racers;
    std::uint64_t seed = 0;
    int lapCount = 3;
    RespawnRule respawn;
};

}
