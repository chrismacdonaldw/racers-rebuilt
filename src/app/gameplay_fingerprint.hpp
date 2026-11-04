#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "mods/mod_list.hpp"
#include "settings/racers.hpp"
#include "settings/registry.hpp"
#include "sim/powerup/powerup_tuning.hpp"

namespace rr::app {

struct PlayerRacers {
    std::optional<settings::Racer> first;
    std::optional<settings::Racer> second;
};

/// Mods, overrides, changed power-ups, gameplay settings away from their defaults, then the
/// players' racers.
[[nodiscard]] std::vector<std::string> GameplayFingerprint(
    const settings::Registry& settings, const mods::ModList& mods,
    std::optional<std::uint64_t> overridesHash, const sim::PowerupTuning& powerups,
    const PlayerRacers& players);

}
