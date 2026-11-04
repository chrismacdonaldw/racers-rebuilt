#pragma once

#include <chrono>

#include "app/ai_skill_settings.hpp"
#include "app/race_scene_name.hpp"
#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"
#include "sim/ai/ai_craft.hpp"
#include "sim/race/race_setup.hpp"
#include "sim/race/respawn.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kRespawn{.key = "race.respawn",
                                                      .defaultValue = false,
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

inline constexpr settings::SettingSpec<int> kAiPace{
    .key = "race.ai_pace",
    .defaultValue = kDefaultAiPace,
    .range = settings::SettingRange<int>{.min = kMinAiPace, .max = kMaxAiPace},
    .label = "Physics racers' pace (% of the original)",
    .category = "Enhancements/Gameplay",
    .requirement = settings::SettingRequirement{.key = kAiSkill.key, .choice = "false"}};

inline constexpr settings::SettingSpec<bool> kAiBraking{
    .key = "race.ai_braking",
    .defaultValue = false,
    .label = "Physics racers brake before corners",
    .category = "Enhancements/Gameplay"};

inline constexpr settings::SettingSpec<bool> kAiPassing{
    .key = "race.ai_passing",
    .defaultValue = false,
    .label = "Physics racers steer around rivals",
    .category = "Enhancements/Gameplay"};

inline constexpr settings::SettingSpec<bool> kAiUnsticking{
    .key = "race.ai_unstick",
    .defaultValue = false,
    .label = "Physics racers back out when stuck",
    .category = "Enhancements/Gameplay"};

inline constexpr settings::SettingSpec<bool> kAiAiming{
    .key = "race.ai_aiming",
    .defaultValue = false,
    .label = "Physics racers line up shots and drops",
    .category = "Enhancements/Gameplay"};

inline constexpr settings::SettingSpec<bool> kAiRacingLine{
    .key = "race.ai_racing_line",
    .defaultValue = false,
    .label = "Physics racers drive a racing line at their car's speed",
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
    registry.Declare(kAiPace);
    registry.Declare(kAiBraking);
    registry.Declare(kAiPassing);
    registry.Declare(kAiUnsticking);
    registry.Declare(kAiAiming);
    DeclareAiSkillSettings(registry);
    registry.Declare(kAiRacingLine);
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

[[nodiscard]] inline sim::AiCraft AiCraftFrom(const settings::Registry& registry) {
    return {.brakes = registry.Get(kAiBraking),
            .passes = registry.Get(kAiPassing),
            .unsticks = registry.Get(kAiUnsticking),
            .aims = registry.Get(kAiAiming),
            .racingLine = registry.Get(kAiRacingLine)};
}

/// Only the respawn and computer racer fields are set.
[[nodiscard]] inline RaceSceneSetup RaceRulesFrom(const settings::Registry& registry) {
    return {.respawn = RespawnRuleFrom(registry),
            .aiDriving = AiDrivingFrom(registry),
            .aiPace = registry.Get(kAiPace),
            .aiCraft = AiCraftFrom(registry),
            .aiSkill = AiSkillFrom(registry)};
}

}
