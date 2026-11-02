#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <string_view>

#include "app/menu/award_scene_name.hpp"
#include "assets/circuit_database.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "settings/progress.hpp"
#include "sim/race/circuit_run.hpp"

namespace rr::app {

inline constexpr std::string_view kProgressFileName = "progress.toml";

/// Returns whether `progress` changed.
bool GrantAward(settings::Progress& progress, const assets::CircuitEntry& circuit,
                sim::CircuitAward award);

/// Writes `file` only when the award changed it, and never over a file that doesn't parse.
[[nodiscard]] Result<void> SaveAward(const std::filesystem::path& file, const assets::Vfs& vfs,
                                     const AwardSceneSetup& award);

/// Logs a file that doesn't parse and treats it as a fresh game.
[[nodiscard]] std::set<std::string> UnlockedCircuits(const std::filesystem::path& file);

}
