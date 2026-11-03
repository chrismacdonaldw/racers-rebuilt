#pragma once

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

/// `mod:<id>@<version>` for each enabled mod, then `racer:` and `racer2:` `<name>@<hash>` lines.
[[nodiscard]] std::vector<std::string> GameplayFingerprint(const settings::Registry& settings,
                                                           const mods::ModList& mods,
                                                           const PlayerRacers& players);

}
