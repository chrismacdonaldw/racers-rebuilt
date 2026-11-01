#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/bounding_sphere.hpp"
#include "sim/ai/powerup_habits.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/powerup/powerup_effect.hpp"
#include "sim/race/respawn.hpp"

namespace rr::sim {

inline constexpr BoundingSphere kTypicalRacerBounds{.centre = {.x = -0.88F, .y = 0.01F, .z = 1.97F},
                                                    .radius = 6.7F};

struct RacerEntry {
    ChassisSpec chassis;
    CarBuild build;
    BoundingSphere localBounds = kTypicalRacerBounds;
    PowerupHabits habits;
    bool isPlayer = false;
};

/// Racer `i` starts in grid slot `i`.
struct RaceSetup {
    std::string track;
    std::vector<RacerEntry> racers;
    std::uint64_t seed = 0;
    int lapCount = 3;
    RespawnRule respawn;
    EffectFactories effects;
};

}
