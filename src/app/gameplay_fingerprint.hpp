#pragma once

#include <string>
#include <vector>

#include "mods/mod_list.hpp"
#include "settings/registry.hpp"

namespace rr::app {

/// `mod:<id>@<version>` for each enabled mod.
[[nodiscard]] std::vector<std::string> GameplayFingerprint(const settings::Registry& settings,
                                                           const mods::ModList& mods);

}
