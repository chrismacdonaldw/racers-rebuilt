#include "app/viewer_scene.hpp"

#include "app/car_viewer_scene.hpp"
#include "app/track_viewer_scene.hpp"

namespace rr::app {

Result<std::unique_ptr<Scene>> CreateViewerScene(render::Renderer& renderer, const assets::Vfs& vfs,
                                                 assets::TrackLoader& loader,
                                                 std::string_view scene, Nanoseconds started) {
    if (IsCarViewerScene(scene)) {
        return CarViewerScene::Create(renderer, vfs, loader, scene)
            .transform(ToScene<CarViewerScene>);
    }
    return TrackViewerScene::Create(renderer, vfs, loader, scene, started)
        .transform(ToScene<TrackViewerScene>);
}

}
