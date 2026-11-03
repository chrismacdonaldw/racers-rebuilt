#pragma once

#include <cstddef>
#include <string>

#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "settings/racers.hpp"

namespace rr::app {

/// The first part set's chassis with no pieces, until the driver and car editors exist.
[[nodiscard]] Result<settings::Racer> NewRacer(const assets::Vfs& vfs, std::string name);

/// Returns the copy's index. Like the original's, the copy keeps the name and trophies.
std::size_t CopyRacer(settings::RacerStore& store, std::size_t racer);

/// Keeps `selected` on the same racer, or clears it when that racer goes.
void DeleteRacer(settings::RacerStore& store, std::size_t racer);

}
