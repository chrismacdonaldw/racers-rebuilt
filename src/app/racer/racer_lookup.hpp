#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include "assets/original_save.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "settings/racers.hpp"

namespace rr::app {

/// Leaves the trophies empty.
[[nodiscard]] settings::Racer RacerFromSave(const assets::SavedRacer& saved);

/// Looks in the game's `DEFAULT.LRS`, then `QBUILD.LRS`. Names match in any case.
[[nodiscard]] Result<settings::Racer> FindGameRacer(const assets::Vfs& vfs, std::string_view name);

/// Looks in `racersFile` before the game's racers.
[[nodiscard]] Result<settings::Racer> FindRacer(const assets::Vfs& vfs,
                                                const std::filesystem::path& racersFile,
                                                std::string_view name);

[[nodiscard]] Result<std::optional<settings::Racer>> SelectedRacer(
    const std::filesystem::path& racersFile);

/// The game's `CHAMP` quick build on `chassis`, the car a champion hands over.
[[nodiscard]] Result<settings::Racer> FindChampionCar(const assets::Vfs& vfs,
                                                      std::string_view chassis);

}
