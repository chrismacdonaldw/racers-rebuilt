#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/brick_layout.hpp"
#include "assets/checkpoints.hpp"
#include "assets/hazard_list.hpp"
#include "assets/race_route.hpp"
#include "assets/start_grid.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::assets {

inline constexpr std::string_view kTrackModelFile = "TRACK.GLB";
inline constexpr std::size_t kFullGrid = 6;

struct SlotRoute {
    std::size_t slot = 0;
    RaceRoute route;
};

/// A kind the file has no markers for stays unset, so the track's own binary file is read instead.
struct TrackMarkers {
    std::optional<StartGridAsset> grid;
    std::optional<CheckpointsAsset> gates;
    std::optional<BrickLayout> bricks;
    std::optional<BrickLayout> trialBricks;
    std::optional<std::vector<Hazard>> hazards;
    std::vector<SlotRoute> routes;
    std::vector<std::string> problems;
};

/// Markers are named as `--export-track` writes them. A broken one is left out with a problem.
[[nodiscard]] Result<TrackMarkers> ParseTrackMarkers(std::span<const std::byte> glb);

/// Unset when `track` has no `TRACK.GLB`.
[[nodiscard]] Result<std::optional<TrackMarkers>> ReadTrackMarkers(const Vfs& vfs,
                                                                   std::string_view track);

void MirrorY(TrackMarkers& markers);

/// Each track `TRACK.GLB`'s broken markers, and a start grid or gates it has in neither form.
[[nodiscard]] std::vector<std::string> TrackMarkerProblems(const Vfs& vfs);

}
