#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/race_route.hpp"
#include "assets/track_markers.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::sim {

/// Per pace `F`, `M`, `S`: `R<slot>_<pace>_0.RRB` upward to the first gap, at most 10 each.
[[nodiscard]] std::vector<std::string> SlotRoutes(std::span<const assets::VfsEntry> folder,
                                                  int slot);

/// Every route each grid slot may drive, indexed by slot.
using GridRoutes = std::vector<std::vector<assets::RaceRoute>>;

/// `track` is a race track folder under `GAMEDATA`, such as `RACEC0R0`.
[[nodiscard]] Result<assets::RaceRoute> LoadRoute(const assets::Vfs& vfs, std::string_view track,
                                                  std::string_view name, bool isMirrored);

/// Routes past `slotCount` are dropped.
[[nodiscard]] GridRoutes GridRoutesOf(std::span<const assets::SlotRoute> routes,
                                      std::size_t slotCount);

[[nodiscard]] Result<GridRoutes> LoadGridRoutes(const assets::Vfs& vfs, std::string_view track,
                                                std::size_t slotCount, bool isMirrored);

}
