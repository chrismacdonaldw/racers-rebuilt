#pragma once

#include <filesystem>
#include <span>
#include <vector>

#include "assets/original_save.hpp"
#include "assets/race_catalog.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "settings/progress.hpp"
#include "settings/racers.hpp"

namespace rr::app {

/// Keeps the better of each held and saved record. Returns whether anything changed.
bool MergeOriginalSave(settings::Progress& progress, const assets::OriginalSave& save,
                       const assets::RaceCatalog& catalog);

/// Adds each racer, with its trophies, unless the store holds one exactly like it.
bool MergeSavedRacers(settings::RacerStore& store, std::span<const assets::SavedRacer> saved,
                      const assets::RaceCatalog& catalog);

/// Returns whether the save added anything, racers included. Never writes over a bad file.
[[nodiscard]] Result<bool> ImportOriginalSave(const std::filesystem::path& saveFile,
                                              const std::filesystem::path& progressFile,
                                              const assets::Vfs& vfs);

/// The `Save/<n>/LEGORac1` files under the original's `install` folder, by number.
[[nodiscard]] std::vector<std::filesystem::path> FindOriginalSaves(
    const std::filesystem::path& install);

}
