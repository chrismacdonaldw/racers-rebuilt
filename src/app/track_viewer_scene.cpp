#include "app/track_viewer_scene.hpp"

#include <cmath>
#include <utility>

#include "app/track_model.hpp"
#include "core/log.hpp"
#include "platform/clock.hpp"

namespace rr::app {
namespace {

constexpr float kNearPlaneRadii = 0.001F;
constexpr float kFarPlaneRadii = 8.0F;

render::Camera FitCamera(const Bounds& bounds) {
    const Vector3 size{.x = bounds.max.x - bounds.min.x,
                       .y = bounds.max.y - bounds.min.y,
                       .z = bounds.max.z - bounds.min.z};
    const float radius =
        std::sqrt((size.x * size.x) + (size.y * size.y) + (size.z * size.z)) / 2.0F;
    render::Camera camera;
    camera.transform = OverviewCamera(bounds, camera.verticalFovDegrees);
    camera.nearPlane = radius * kNearPlaneRadii;
    camera.farPlane = radius * kFarPlaneRadii;
    return camera;
}

}

Result<std::unique_ptr<TrackViewerScene>> TrackViewerScene::Create(render::Renderer& renderer,
                                                                   assets::TrackLoader& loader,
                                                                   std::string_view track,
                                                                   Nanoseconds started) {
    Result<TrackModel> model = loader.Load(std::string("GAMEDATA/") + std::string(track))
                                   .and_then([&](const assets::TrackAssets& assets) {
                                       return UploadTrack(renderer, assets);
                                   });
    if (!model) {
        return std::unexpected(std::move(model.error()));
    }
    if (model->bounds.isEmpty) {
        return Fail(ErrorCode::InvalidData, std::format("{} places no models", track));
    }
    return std::unique_ptr<TrackViewerScene>(
        new TrackViewerScene({.renderer = &renderer,
                              .track = std::string(track),
                              .draws = std::move(model->draws),
                              .camera = FitCamera(model->bounds),
                              .started = started}));
}

void TrackViewerScene::Tick([[maybe_unused]] const sim::InputFrame& input) {}

void TrackViewerScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {.transform = parts_.camera.transform};
}

void TrackViewerScene::Render(const sim::RenderSnapshot& interpolated) {
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
