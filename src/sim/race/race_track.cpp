#include "sim/race/race_track.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <utility>

#include "assets/checkpoints.hpp"
#include "assets/start_grid.hpp"

namespace rr::sim {
namespace {

template <typename Parse>
auto ReadParsed(const assets::Vfs& vfs, std::string_view track, std::string_view file,
                Parse parse) {
    const std::string path = std::format("GAMEDATA/{}/{}", track, file);
    return vfs.Read(path)
        .and_then([&](const assets::FileData& data) { return parse(data.Bytes()); })
        .transform_error([&](Error error) {
            error.message = std::format("{}: {}", path, error.message);
            return error;
        });
}

std::vector<RacerStart> GridStarts(const assets::StartGridAsset& grid) {
    std::vector<RacerStart> starts;
    starts.reserve(grid.slots.size());
    for (const assets::StartSlot& slot : grid.slots) {
        starts.push_back({.position = slot.position,
                          .headingRadians = std::atan2(slot.forward.y, slot.forward.x)});
    }
    return starts;
}

float LowestPoint(const CollisionWorld& world) {
    const std::vector<Vector3>& vertices = world.Mesh().Asset().vertices;
    return std::ranges::fold_left(
        vertices, std::numeric_limits<float>::max(),
        [](float lowest, const Vector3& vertex) { return std::min(lowest, vertex.z); });
}

}

Result<CheckpointGraph> LoadCheckpoints(const assets::Vfs& vfs, std::string_view track) {
    return ReadParsed(vfs, track, "RACE.CPB", assets::ParseCpb)
        .and_then([](assets::CheckpointsAsset asset) {
            return CheckpointGraph::Create(std::move(asset));
        });
}

Result<RaceTrack> LoadRaceTrack(const assets::Vfs& vfs, std::string_view track) {
    Result<CheckpointGraph> checkpoints = LoadCheckpoints(vfs, track);
    Result<assets::StartGridAsset> grid = checkpoints.and_then([&](const CheckpointGraph&) {
        return ReadParsed(vfs, track, "STARTPOS.SPB", assets::ParseSpb);
    });
    Result<TrackCollision> collision = grid.and_then(
        [&](const assets::StartGridAsset&) { return LoadTrackCollision(vfs, track); });
    Result<GridRoutes> routes = collision.and_then([&](const TrackCollision&) {
        return LoadGridRoutes(vfs, track, grid->slots.size(), false);
    });
    if (!routes) {
        return std::unexpected(std::move(routes.error()));
    }
    const float lowestTrackZ = LowestPoint(collision->Track());
    return RaceTrack{.collision = std::move(*collision),
                     .checkpoints = std::move(*checkpoints),
                     .grid = GridStarts(*grid),
                     .routes = std::move(*routes),
                     .lowestTrackZ = lowestTrackZ};
}

}
