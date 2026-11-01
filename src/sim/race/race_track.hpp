#pragma once

#include <string_view>
#include <vector>

#include "assets/brick_layout.hpp"
#include "assets/target_points.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "sim/ai/route_choice.hpp"
#include "sim/collision/track_collision.hpp"
#include "sim/physics/racer.hpp"
#include "sim/race/checkpoint_graph.hpp"
#include "sim/track/track_events.hpp"

namespace rr::sim {

struct RaceTrack {
    TrackCollision collision;
    CheckpointGraph checkpoints;
    std::vector<RacerStart> grid;
    GridRoutes routes;
    assets::BrickLayout bricks;
    std::vector<assets::TargetPoint> targets;
    TrackEventSources events;
    float lowestTrackZ = 0.0F;
};

/// `track` is a race track folder under `GAMEDATA`, such as `RACEC0R0`.
[[nodiscard]] Result<CheckpointGraph> LoadCheckpoints(const assets::Vfs& vfs,
                                                      std::string_view track);

/// `track` is a race track folder under `GAMEDATA`, such as `RACEC0R0`.
[[nodiscard]] Result<RaceTrack> LoadRaceTrack(const assets::Vfs& vfs, std::string_view track);

}
