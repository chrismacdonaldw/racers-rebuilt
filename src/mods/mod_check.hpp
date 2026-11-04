#pragma once

#include <string>
#include <vector>

#include "core/result.hpp"
#include "mods/mod_list.hpp"
#include "settings/registry.hpp"

namespace rr::mods {

/// Mods skipped for a broken manifest, then mods that are on but held back, each with its file.
[[nodiscard]] std::vector<std::string> ModProblems(const ModList& mods,
                                                   const settings::Registry& registry);

/// Logs every problem. Fails with all of them when there are any.
[[nodiscard]] Result<void> CheckMods(const ModList& mods, const settings::Registry& registry);

}
