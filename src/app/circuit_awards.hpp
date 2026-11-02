#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "app/menu/award_scene_name.hpp"
#include "assets/circuit_database.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "settings/progress.hpp"
#include "settings/racers.hpp"
#include "sim/race/circuit_run.hpp"

namespace rr::app {

inline constexpr std::string_view kProgressFileName = "progress.toml";
inline constexpr std::string_view kRacersFileName = "racers.toml";

/// Returns whether `progress` or `trophies` changed.
bool GrantAward(settings::Progress& progress, settings::Trophies& trophies,
                const assets::CircuitEntry& circuit, sim::CircuitAward award);

[[nodiscard]] bool OpensNewCircuit(const settings::Progress& progress,
                                   const assets::CircuitEntry& circuit, sim::CircuitAward award);

/// Returns whether a new circuit opened. Writes only on a change, and never over a bad file.
[[nodiscard]] Result<bool> SaveAward(const std::filesystem::path& file, const assets::Vfs& vfs,
                                     const AwardSceneSetup& award);

/// Logs a file that doesn't parse and treats it as a fresh game.
[[nodiscard]] settings::Progress ReadProgress(const std::filesystem::path& file);

}
