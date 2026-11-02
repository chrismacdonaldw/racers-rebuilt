#pragma once

#include <memory>
#include <string_view>

#include "app/scene.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// The car viewer for `car:<racer name>`, otherwise the track viewer for a `GAMEDATA` folder.
[[nodiscard]] Result<std::unique_ptr<Scene>> CreateViewerScene(render::Renderer& renderer,
                                                               const assets::Vfs& vfs,
                                                               assets::TrackLoader& loader,
                                                               std::string_view scene,
                                                               Nanoseconds started);

}
