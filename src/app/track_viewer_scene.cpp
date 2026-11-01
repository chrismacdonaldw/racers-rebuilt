#include "app/track_viewer_scene.hpp"

#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/checkpoint_gates.hpp"
#include "app/collision_wireframe.hpp"
#include "app/frame_loop.hpp"
#include "app/track_model.hpp"
#include "app/track_space.hpp"
#include "core/log.hpp"
#include "platform/clock.hpp"
#include "sim/collision/track_collision.hpp"
#include "sim/race/race_track.hpp"

namespace rr::app {
namespace {

constexpr sim::EntityId kPaceCarId{1};

std::vector<render::DebugLine> CollisionLines(const assets::Vfs& vfs, std::string_view track) {
    const Result<sim::TrackCollision> collision = sim::LoadTrackCollision(vfs, track);
    if (!collision) {
        log::Warn("no collision wireframe: {}", collision.error().message);
        return {};
    }
    return BuildCollisionWireframe(*collision);
}

std::unique_ptr<sim::TrackEntitySources> EntitySources(const assets::Vfs& vfs,
                                                       std::string_view track) {
    Result<sim::TrackEntitySources> sources = sim::LoadTrackEntities(vfs, track);
    if (!sources) {
        log::Warn("animated entities stand still: {}", sources.error().message);
        return std::make_unique<sim::TrackEntitySources>();
    }
    return std::make_unique<sim::TrackEntitySources>(std::move(*sources));
}

}

Result<std::unique_ptr<TrackViewerScene>> TrackViewerScene::Create(render::Renderer& renderer,
                                                                   const assets::Vfs& vfs,
                                                                   assets::TrackLoader& loader,
                                                                   std::string_view track,
                                                                   Nanoseconds started) {
    const std::string folder = std::format("GAMEDATA/{}", track);
    Result<TrackModel> model = loader.Load(folder).and_then(
        [&](const assets::TrackAssets& assets) { return UploadTrack(renderer, assets); });
    Result<std::vector<render::MeshDraw>> car =
        model.and_then([&](const TrackModel&) { return UploadStandInCar(renderer, loader); });
    Result<assets::RaceRoute> route = car.and_then(
        [&](const std::vector<render::MeshDraw>&) { return LoadPaceRoute(vfs, folder); });
    if (!route) {
        return std::unexpected(std::move(route.error()));
    }
    if (model->bounds.isEmpty) {
        return Fail(ErrorCode::InvalidData, std::format("{} places no models", track));
    }
    Result<sim::CheckpointGraph> loaded = sim::LoadCheckpoints(vfs, track);
    std::optional<sim::CheckpointGraph> checkpoints;
    if (loaded) {
        checkpoints = std::move(*loaded);
    } else {
        log::Warn("{}: no lap counting, {}", track, loaded.error().message);
    }
    const std::size_t firstCarDraw = model->draws.size();
    model->draws.insert(model->draws.end(), car->begin(), car->end());
    std::unique_ptr<sim::TrackEntitySources> entitySources = EntitySources(vfs, track);
    sim::TrackEntities entities(*entitySources);
    TrackEntityDraws entityDraws(model->animatedModels, *entitySources);
    return std::unique_ptr<TrackViewerScene>(
        new TrackViewerScene({.renderer = &renderer,
                              .track = std::string(track),
                              .draws = std::move(model->draws),
                              .firstCarDraw = firstCarDraw,
                              .paceCar = PaceCar(std::move(*route)),
                              .camera = TrackCamera(model->bounds),
                              .collisionLines = CollisionLines(vfs, track),
                              .started = started,
                              .checkpoints = std::move(checkpoints),
                              .entitySources = std::move(entitySources),
                              .entities = std::move(entities),
                              .entityDraws = std::move(entityDraws)}));
}

void TrackViewerScene::ApplySettings(const settings::Registry& settings) {
    showsCheckpoints_ = settings.Get(kShowCheckpoints);
    showsCollision_ = settings.Get(kCollisionWireframe);
}

void TrackViewerScene::Tick([[maybe_unused]] const sim::InputFrame& input) {
    const Transform before = parts_.paceCar.GamePose();
    parts_.paceCar.Tick();
    parts_.entities.Tick(kSimTick);
    if (parts_.checkpoints) {
        TrackLaps(*parts_.checkpoints, before);
    }
}

void TrackViewerScene::TrackLaps(const sim::CheckpointGraph& checkpoints, const Transform& before) {
    const Transform after = parts_.paceCar.GamePose();
    const int laps = laps_.CompletedLaps();
    if (const auto crossing = checkpoints.FindCrossing(before.position, after.position)) {
        laps_.Cross(checkpoints, *crossing);
    }
    laps_.Update(checkpoints, kSimTick, Apply({.rotation = after.rotation}, {.x = 1.0F}));
    if (laps_.CompletedLaps() != laps) {
        log::Info("{}: pace car finished lap {}", parts_.track, laps_.CompletedLaps());
    }
}

void TrackViewerScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.assign(1, {.id = kPaceCarId, .transform = parts_.paceCar.Car()});
    out.camera = {.transform = parts_.paceCar.Camera()};
    out.entityClocks.assign(parts_.entities.Clocks().begin(), parts_.entities.Clocks().end());
}

void TrackViewerScene::Render(const sim::RenderSnapshot& interpolated) {
    if (!interpolated.entities.empty()) {
        for (std::size_t i = parts_.firstCarDraw; i < parts_.draws.size(); ++i) {
            parts_.draws[i].transform = interpolated.entities.front().transform;
        }
    }
    render::Camera camera = parts_.camera;
    camera.transform = interpolated.camera.transform;
    debugLines_.clear();
    if (showsCheckpoints_ && parts_.checkpoints) {
        DrawGates(*parts_.checkpoints, laps_, debugLines_);
    }
    if (showsCollision_) {
        debugLines_.insert(debugLines_.end(), parts_.collisionLines.begin(),
                           parts_.collisionLines.end());
    }
    parts_.entityDraws.Update(interpolated.entityClocks);
    frameDraws_.assign(parts_.draws.begin(), parts_.draws.end());
    frameDraws_.insert(frameDraws_.end(), parts_.entityDraws.Draws().begin(),
                       parts_.entityDraws.Draws().end());
    Result<void> drawn =
        parts_.renderer->Draw({.camera = camera, .meshes = frameDraws_, .lines = debugLines_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
        return;
    }
    if (!hasDrawn_) {
        hasDrawn_ = true;
        log::Info(
            "{}: {} draws, first frame {} ms after loading began", parts_.track, frameDraws_.size(),
            std::chrono::duration_cast<Milliseconds>(platform::Now() - parts_.started).count());
    }
}

}
