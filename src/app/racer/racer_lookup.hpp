#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "app/racer/racer_ref.hpp"
#include "assets/original_save.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "settings/racers.hpp"

namespace rr::app {

/// Leaves the trophies empty.
[[nodiscard]] settings::Racer RacerFromSave(const assets::SavedRacer& saved);

/// Looks in the game's `DEFAULT.LRS`, then `QBUILD.LRS`.
[[nodiscard]] Result<settings::Racer> FindGameRacer(const assets::Vfs& vfs, const RacerRef& ref);

/// Looks in `racersFile` before the game's racers, and logs a `racersFile` that won't load.
[[nodiscard]] Result<settings::Racer> FindRacer(const assets::Vfs& vfs,
                                                const std::filesystem::path& racersFile,
                                                const RacerRef& ref);

/// None for a nameless `ref`, or one nobody has.
[[nodiscard]] std::optional<settings::Racer> FindPlayerRacer(
    const assets::Vfs& vfs, const std::filesystem::path& racersFile, const RacerRef& ref);

[[nodiscard]] Result<std::vector<settings::Racer>> ReadDefaultRacers(const assets::Vfs& vfs);

[[nodiscard]] Result<std::optional<settings::Racer>> SelectedRacer(
    const std::filesystem::path& racersFile);

/// Logs a racers file that doesn't load, and gives none for it.
[[nodiscard]] std::optional<settings::Racer> SelectedRacerOrNone(
    const std::filesystem::path& racersFile);

/// The game's `CHAMP` quick build on `chassis`, the car a champion hands over.
[[nodiscard]] Result<settings::Racer> FindChampionCar(const assets::Vfs& vfs,
                                                      std::string_view chassis);

}
