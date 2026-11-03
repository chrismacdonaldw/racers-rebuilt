#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "mods/mod_list.hpp"
#include "settings/racers.hpp"
#include "settings/registry.hpp"

namespace rr::app {

struct PlayerRacers {
    std::optional<settings::Racer> first;
    std::optional<settings::Racer> second;
};

/// `mod:<id>@<version>` per enabled mod, `overrides:<hash>`, then `racer:` and `racer2:` lines.
[[nodiscard]] std::vector<std::string> GameplayFingerprint(
    const settings::Registry& settings, const mods::ModList& mods,
    std::optional<std::uint64_t> overridesHash, const PlayerRacers& players);

}
