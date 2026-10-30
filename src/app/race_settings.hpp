#pragma once

#include <chrono>

#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"
#include "sim/race/respawn.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kRespawn{.key = "race.respawn",
                                                      .defaultValue = true,
                                                      .label = "Respawn off the track",
                                                      .category = "Race"};

inline constexpr settings::SettingSpec<int> kRespawnSeconds{
    .key = "race.respawn_seconds",
    .defaultValue = 3,
    .range = settings::SettingRange<int>{.min = 1, .max = 30},
    .label = "Seconds in the air before respawning",
    .category = "Race"};

inline void DeclareRaceSettings(settings::Registry& registry) {
    registry.Declare(kRespawn);
    registry.Declare(kRespawnSeconds);
}

[[nodiscard]] inline sim::RespawnRule RespawnRuleFrom(const settings::Registry& registry) {
    return {.isEnabled = registry.Get(kRespawn),
            .timeout = std::chrono::seconds{registry.Get(kRespawnSeconds)}};
}

}
