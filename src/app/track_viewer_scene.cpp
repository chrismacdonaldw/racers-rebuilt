#include "app/track_viewer_scene.hpp"

#include <cmath>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include "app/track_model.hpp"
#include "core/log.hpp"
#include "platform/clock.hpp"

namespace rr::app {
namespace {

constexpr float kNearPlaneRadii = 0.001F;
constexpr float kFarPlaneRadii = 8.0F;
constexpr std::string_view kPaceCarFolder = "MENUDATA/CIRCUIT1";
constexpr std::string_view kPaceCarModel = "CR_CAR";
constexpr sim::EntityId kPaceCarId{1};

render::Camera CameraFor(const Bounds& bounds) {
    const Vector3 size{.x = bounds.max.x - bounds.min.x,
                       .y = bounds.max.y - bounds.min.y,
                       .z = bounds.max.z - bounds.min.z};
    const float radius =
        std::sqrt((size.x * size.x) + (size.y * size.y) + (size.z * size.z)) / 2.0F;
    render::Camera camera;
    camera.nearPlane = radius * kNearPlaneRadii;
    camera.farPlane = radius * kFarPlaneRadii;
    return camera;
}

Result<std::vector<render::MeshDraw>> UploadPaceCar(render::Renderer& renderer,
                                                    assets::TrackLoader& loader) {
    return loader.Load(kPaceCarFolder).and_then([&](const assets::TrackAssets& assets) {
        return UploadModel(renderer, assets, kPaceCarModel);
    });
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
        model.and_then([&](const TrackModel&) { return UploadPaceCar(renderer, loader); });
    Result<assets::RaceRoute> route = car.and_then(
        [&](const std::vector<render::MeshDraw>&) { return LoadPaceRoute(vfs, folder); });
    if (!route) {
        return std::unexpected(std::move(route.error()));
    }
    if (model->bounds.isEmpty) {
        return Fail(ErrorCode::InvalidData, std::format("{} places no models", track));
    }
    const std::size_t firstCarDraw = model->draws.size();
    model->draws.insert(model->draws.end(), car->begin(), car->end());
    return std::unique_ptr<TrackViewerScene>(
        new TrackViewerScene({.renderer = &renderer,
                              .track = std::string(track),
                              .draws = std::move(model->draws),
                              .firstCarDraw = firstCarDraw,
                              .paceCar = PaceCar(std::move(*route)),
                              .camera = CameraFor(model->bounds),
                              .started = started}));
}

void TrackViewerScene::Tick([[maybe_unused]] const sim::InputFrame& input) {
    parts_.paceCar.Tick();
}

void TrackViewerScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.assign(1, {.id = kPaceCarId, .transform = parts_.paceCar.Car()});
    out.camera = {.transform = parts_.paceCar.Camera()};
}

void TrackViewerScene::Render(const sim::RenderSnapshot& interpolated) {
    if (!interpolated.entities.empty()) {
        for (std::size_t i = parts_.firstCarDraw; i < parts_.draws.size(); ++i) {
            parts_.draws[i].transform = interpolated.entities.front().transform;
        }
    }
    render::Camera camera = parts_.camera;
    camera.transform = interpolated.camera.transform;
    Result<void> drawn = parts_.renderer->Draw({.camera = camera, .meshes = parts_.draws});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
        return;
    }
    if (!hasDrawn_) {
        hasDrawn_ = true;
        log::Info(
            "{}: {} draws, first frame {} ms after loading began", parts_.track,
            parts_.draws.size(),
            std::chrono::duration_cast<Milliseconds>(platform::Now() - parts_.started).count());
    }
}

}
