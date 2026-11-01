#include "sim/race/race_track.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <utility>

#include "assets/brick_layout.hpp"
#include "assets/checkpoints.hpp"
#include "assets/game_path.hpp"
#include "assets/start_grid.hpp"
#include "assets/target_points.hpp"
#include "assets/track_triggers.hpp"

namespace rr::sim {
namespace {

constexpr std::string_view kRacerTriggers = "MAINTRIG.TRB";
constexpr std::string_view kRenamedRacerTriggers = "NEWTRIG.TRB";

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

Result<std::vector<assets::TargetPoint>> LoadTargetPoints(const assets::Vfs& vfs,
                                                          std::string_view track) {
    const std::string name = std::format("{}.TGB", assets::ToUpperAscii(track));
    return vfs.List(std::format("GAMEDATA/{}", track))
        .and_then([&](const std::vector<assets::VfsEntry>& folder)
                      -> Result<std::vector<assets::TargetPoint>> {
            if (!assets::HasFile(folder, name)) {
                return {};
            }
            return ReadParsed(vfs, track, name, assets::ParseTgb);
        });
}

auto SphereParser(assets::TriggerList list) {
    return [list](std::span<const std::byte> bytes) {
        return assets::ParseTriggerSpheres(bytes, list);
    };
}

Result<TrackEventSources> LoadEventSources(const assets::Vfs& vfs, std::string_view track) {
    Result<std::vector<assets::TriggerSphere>> racer =
        vfs.List(std::format("GAMEDATA/{}", track))
            .and_then([&](const std::vector<assets::VfsEntry>& folder) {
                const std::string_view name = assets::HasFile(folder, kRenamedRacerTriggers)
                                                  ? kRenamedRacerTriggers
                                                  : kRacerTriggers;
                return ReadParsed(vfs, track, name, SphereParser(assets::TriggerList::Racer));
            });
    Result<std::vector<assets::TriggerSphere>> environment =
        racer.and_then([&](const std::vector<assets::TriggerSphere>&) {
            return ReadParsed(vfs, track, "ENVIRON.TRB",
                              SphereParser(assets::TriggerList::Environment));
        });
    Result<std::vector<assets::TrackTimer>> timers =
        environment.and_then([&](const std::vector<assets::TriggerSphere>&) {
            return ReadParsed(vfs, track, "TIMER.TIB", assets::ParseTrackTimers);
        });
    if (!timers) {
        return std::unexpected(std::move(timers.error()));
    }
    return TrackEventSources{.racerSpheres = std::move(*racer),
                             .environmentSpheres = std::move(*environment),
                             .timers = std::move(*timers)};
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
    Result<assets::BrickLayout> bricks = routes.and_then(
        [&](const GridRoutes&) { return ReadParsed(vfs, track, "POWERUP.PWB", assets::ParsePwb); });
    Result<std::vector<assets::TargetPoint>> targets =
        bricks.and_then([&](const assets::BrickLayout&) { return LoadTargetPoints(vfs, track); });
    Result<TrackEventSources> events = targets.and_then(
        [&](const std::vector<assets::TargetPoint>&) { return LoadEventSources(vfs, track); });
    if (!events) {
        return std::unexpected(std::move(events.error()));
    }
    const float lowestTrackZ = LowestPoint(collision->Track());
    return RaceTrack{.collision = std::move(*collision),
                     .checkpoints = std::move(*checkpoints),
                     .grid = GridStarts(*grid),
                     .routes = std::move(*routes),
                     .bricks = std::move(*bricks),
                     .targets = std::move(*targets),
                     .events = std::move(*events),
                     .lowestTrackZ = lowestTrackZ};
}

}
