#pragma once

#include <chrono>

#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"
#include "sim/race/race_setup.hpp"
#include "sim/race/respawn.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kRespawn{.key = "race.respawn",
                                                      .defaultValue = true,
                                                      .label = "Respawn off the track",
                                                      .category = "Enhancements/Gameplay",
                                                      .changesGameplay = true};

inline constexpr settings::SettingSpec<int> kRespawnSeconds{
    .key = "race.respawn_seconds",
    .defaultValue = 3,
    .range = settings::SettingRange<int>{.min = 1, .max = 30},
    .label = "Seconds in the air before respawning",
    .category = "Enhancements/Gameplay",
    .changesGameplay = true};

inline constexpr settings::SettingSpec<bool> kPhysicsAi{
    .key = "race.physics_ai",
    .defaultValue = false,
    .label = "Computer racers drive with the car physics",
    .category = "Enhancements/Gameplay"};

inline constexpr settings::SettingSpec<int> kOpponents{
    .key = "race.opponents",
    .defaultValue = 5,
    .range = settings::SettingRange<int>{.min = 0, .max = 5},
    .label = "Computer racers in a single race",
    .category = "Race"};

inline constexpr settings::SettingSpec<int> kVersusLaps{
    .key = "race.versus_laps",
    .defaultValue = 3,
    .range = settings::SettingRange<int>{.min = 1, .max = 5},
    .label = "Laps in a versus race",
    .category = "Race"};

inline void DeclareRaceSettings(settings::Registry& registry) {
    registry.Declare(kRespawn);
    registry.Declare(kRespawnSeconds);
    registry.Declare(kPhysicsAi);
    registry.Declare(kOpponents);
    registry.Declare(kVersusLaps);
}

[[nodiscard]] inline sim::RespawnRule RespawnRuleFrom(const settings::Registry& registry) {
    return {.isEnabled = registry.Get(kRespawn),
            .timeout = std::chrono::seconds{registry.Get(kRespawnSeconds)}};
}

[[nodiscard]] inline sim::AiDriving AiDrivingFrom(const settings::Registry& registry) {
    return registry.Get(kPhysicsAi) ? sim::AiDriving::Physics : sim::AiDriving::Route;
}

}
