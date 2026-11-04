#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "assets/game_version.hpp"
#include "core/bounding_sphere.hpp"
#include "core/duration.hpp"
#include "sim/ai/ai_craft.hpp"
#include "sim/ai/ai_skill.hpp"
#include "sim/ai/powerup_habits.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/powerup/powerup_effect.hpp"
#include "sim/race/cheats.hpp"
#include "sim/race/modifiers.hpp"
#include "sim/race/player_cheats.hpp"
#include "sim/race/respawn.hpp"

namespace rr::sim {

inline constexpr BoundingSphere kTypicalRacerBounds{.centre = {.x = -0.88F, .y = 0.01F, .z = 1.97F},
                                                    .radius = 6.7F};

struct RacerEntry {
    ChassisSpec chassis;
    CarBuild build;
    BoundingSphere localBounds = kTypicalRacerBounds;
    PowerupHabits habits;
    Milliseconds suddenStopLength{0};
    bool isPlayer = false;
};

/// `Physics` computer racers drive the car along their route instead of playing it back.
enum class AiDriving : std::uint8_t { Route, Physics };

/// Racer `i` starts in grid slot `firstSlot + i`. A demo's racer 0 finishes it as a player would.
struct RaceSetup {
    std::string track;
    std::string circuit;
    std::vector<RacerEntry> racers;
    std::size_t firstSlot = 0;
    std::uint64_t seed = 0;
    int lapCount = 3;
    RespawnRule respawn;
    AiDriving aiDriving = AiDriving::Route;
    float aiPace = 1.0F;
    AiCraft aiCraft;
    std::optional<AiSkill> aiSkill;
    EffectFactories effects;
    Cheats cheats;
    Modifiers modifiers;
    PlayerCheats playerCheats;
    assets::GameVersion version = assets::GameVersion::Release1999;
    bool lagsInput = false;
    bool isTestDrive = false;
    bool isDemo = false;
};

}
