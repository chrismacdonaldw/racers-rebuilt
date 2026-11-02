#include "sim/race/race_track.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <utility>

#include "assets/brick_layout.hpp"
#include "assets/checkpoints.hpp"
#include "assets/event_table.hpp"
#include "assets/game_path.hpp"
#include "assets/hazard_list.hpp"
#include "assets/mirror.hpp"
#include "assets/start_grid.hpp"
#include "assets/target_points.hpp"
#include "assets/track_triggers.hpp"

namespace rr::sim {
namespace {

constexpr std::string_view kRacerTriggers = "MAINTRIG.TRB";
constexpr std::string_view kRenamedRacerTriggers = "NEWTRIG.TRB";
constexpr std::string_view kEventTable = "EVENT.EVB";

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
                                                          std::string_view track, bool isMirrored) {
    const std::string name = std::format("{}.TGB", assets::ToUpperAscii(track));
    return vfs.List(std::format("GAMEDATA/{}", track))
        .and_then([&](const std::vector<assets::VfsEntry>& folder)
                      -> Result<std::vector<assets::TargetPoint>> {
            if (!assets::HasFile(folder, name)) {
                return {};
            }
            return ReadParsed(vfs, track, name, assets::MirroredIf(isMirrored, assets::ParseTgb));
        });
}

auto SphereParser(assets::TriggerList list) {
    return [list](std::span<const std::byte> bytes) {
        return assets::ParseTriggerSpheres(bytes, list);
    };
}

Result<assets::EventTableAsset> LoadEventTable(const assets::Vfs& vfs, std::string_view track,
                                               bool isMirrored) {
    return vfs.List(std::format("GAMEDATA/{}", track))
        .and_then([&](const std::vector<assets::VfsEntry>& folder) {
            return assets::HasFile(folder, kEventTable)
                       ? ReadParsed(vfs, track, kEventTable,
                                    assets::MirroredIf(isMirrored, assets::ParseEventTable))
                       : Result<assets::EventTableAsset>{};
        });
}

Result<TrackEventSources> LoadEventSources(const assets::Vfs& vfs, std::string_view track,
                                           bool isMirrored) {
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
    Result<assets::EventTableAsset> table =
        timers.and_then([&](const std::vector<assets::TrackTimer>&) {
            return LoadEventTable(vfs, track, isMirrored);
        });
    if (!table) {
        return std::unexpected(std::move(table.error()));
    }
    return TrackEventSources{.racerSpheres = std::move(*racer),
                             .environmentSpheres = std::move(*environment),
                             .timers = std::move(*timers),
                             .table = std::move(*table)};
}

float LowestPoint(const CollisionWorld& world) {
    const std::vector<Vector3>& vertices = world.Mesh().Asset().vertices;
    return std::ranges::fold_left(
        vertices, std::numeric_limits<float>::max(),
        [](float lowest, const Vector3& vertex) { return std::min(lowest, vertex.z); });
}

}

Result<CheckpointGraph> LoadCheckpoints(const assets::Vfs& vfs, std::string_view track,
                                        bool isMirrored) {
    return ReadParsed(vfs, track, "RACE.CPB", assets::MirroredIf(isMirrored, assets::ParseCpb))
        .and_then([](assets::CheckpointsAsset asset) {
            return CheckpointGraph::Create(std::move(asset));
        });
}

RaceCourse CourseOf(const RaceTrack& track, const CollisionQuery& world) {
    return {.checkpoints = &track.checkpoints,
            .grid = track.grid,
            .world = &world,
            .lowestTrackZ = track.lowestTrackZ,
            .routes = &track.routes,
            .bricks = &track.bricks,
            .targets = track.targets,
            .events = &track.events,
            .collision = &track.collision,
            .entities = &track.entities,
            .hazards = track.hazards,
            .isMirrored = track.isMirrored};
}

Result<RaceTrack> LoadRaceTrack(const assets::Vfs& vfs, std::string_view track, bool isMirrored) {
    Result<CheckpointGraph> checkpoints = LoadCheckpoints(vfs, track, isMirrored);
    Result<assets::StartGridAsset> grid = checkpoints.and_then([&](const CheckpointGraph&) {
        return ReadParsed(vfs, track, "STARTPOS.SPB",
                          assets::MirroredIf(isMirrored, assets::ParseSpb));
    });
    Result<TrackCollision> collision = grid.and_then(
        [&](const assets::StartGridAsset&) { return LoadTrackCollision(vfs, track, isMirrored); });
    Result<GridRoutes> routes = collision.and_then([&](const TrackCollision&) {
        return LoadGridRoutes(vfs, track, grid->slots.size(), isMirrored);
    });
    Result<assets::BrickLayout> bricks = routes.and_then([&](const GridRoutes&) {
        return ReadParsed(vfs, track, "POWERUP.PWB",
                          assets::MirroredIf(isMirrored, assets::ParsePwb));
    });
    Result<std::vector<assets::TargetPoint>> targets = bricks.and_then(
        [&](const assets::BrickLayout&) { return LoadTargetPoints(vfs, track, isMirrored); });
    Result<TrackEventSources> events =
        targets.and_then([&](const std::vector<assets::TargetPoint>&) {
            return LoadEventSources(vfs, track, isMirrored);
        });
    Result<TrackEntitySources> entities = events.and_then(
        [&](const TrackEventSources&) { return LoadTrackEntities(vfs, track, isMirrored); });
    Result<std::vector<assets::Hazard>> hazards = entities.and_then([&](const TrackEntitySources&) {
        return ReadParsed(vfs, track, "HAZARDS.HZB",
                          assets::MirroredIf(isMirrored, assets::ParseHazards));
    });
    if (!hazards) {
        return std::unexpected(std::move(hazards.error()));
    }
    const float lowestTrackZ = LowestPoint(collision->Track());
    return RaceTrack{.collision = std::move(*collision),
                     .checkpoints = std::move(*checkpoints),
                     .grid = GridStarts(*grid),
                     .routes = std::move(*routes),
                     .bricks = std::move(*bricks),
                     .targets = std::move(*targets),
                     .events = std::move(*events),
                     .entities = std::move(*entities),
                     .hazards = std::move(*hazards),
                     .lowestTrackZ = lowestTrackZ,
                     .isMirrored = isMirrored};
}

}
