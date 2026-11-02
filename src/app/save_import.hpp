#pragma once

#include <filesystem>
#include <vector>

#include "assets/original_save.hpp"
#include "assets/race_catalog.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "settings/progress.hpp"

namespace rr::app {

/// Keeps the better of each held and saved record and trophy. Returns whether anything changed.
bool MergeOriginalSave(settings::Progress& progress, const assets::OriginalSave& save,
                       const assets::RaceCatalog& catalog);

/// Returns whether the save added anything. Never writes over a progress file that doesn't parse.
[[nodiscard]] Result<bool> ImportOriginalSave(const std::filesystem::path& saveFile,
                                              const std::filesystem::path& progressFile,
                                              const assets::Vfs& vfs);

/// The `Save/<n>/LEGORac1` files under the original's `install` folder, by number.
[[nodiscard]] std::vector<std::filesystem::path> FindOriginalSaves(
    const std::filesystem::path& install);

}
