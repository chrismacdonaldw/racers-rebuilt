#pragma once

#include <optional>
#include <string>
#include <vector>

#include "mods/mod_list.hpp"
#include "settings/racers.hpp"
#include "settings/registry.hpp"

namespace rr::app {

/// `mod:<id>@<version>` for each enabled mod, then `racer:<name>@<hash>` for `player`.
[[nodiscard]] std::vector<std::string> GameplayFingerprint(
    const settings::Registry& settings, const mods::ModList& mods,
    const std::optional<settings::Racer>& player);

}
