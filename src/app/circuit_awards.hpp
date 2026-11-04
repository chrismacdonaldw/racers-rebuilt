#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "app/menu/award_scene_name.hpp"
#include "assets/circuit_database.hpp"
#include "assets/race_catalog.hpp"
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

[[nodiscard]] std::optional<std::int32_t> NewPartSet(const settings::Progress& progress,
                                                     const assets::CircuitEntry& circuit,
                                                     sim::CircuitAward award);

struct AwardUnlocks {
    bool opensCircuit = false;
    std::optional<std::int32_t> partSet;

    friend bool operator==(const AwardUnlocks&, const AwardUnlocks&) = default;
};

/// Writes only on a change, and never over a bad file.
[[nodiscard]] Result<AwardUnlocks> SaveAward(const std::filesystem::path& file,
                                             const assets::Vfs& vfs, const AwardSceneSetup& award);

/// `unlocked`, and each mod circuit that a later circuit in the story is in.
[[nodiscard]] std::set<std::string> OpenCircuits(const assets::RaceCatalog& races,
                                                 const std::set<std::string>& unlocked);

/// The champion is the circuit's boss, else grid slot 1. An empty code where the grid has no such
/// slot.
[[nodiscard]] AwardDrivers DriversOf(const assets::CircuitEntry& circuit, std::size_t racer);

struct AwardClaim {
    std::string screen;
    bool opensCircuit = false;
};

/// Saves what `award` grants and names the screen that shows it. Logs what fails and goes on.
[[nodiscard]] AwardClaim ClaimAward(const std::filesystem::path& file, const assets::Vfs& vfs,
                                    const AwardSceneSetup& award);

/// Logs a file that doesn't parse and treats it as a fresh game.
[[nodiscard]] settings::Progress ReadProgress(const std::filesystem::path& file);

}
