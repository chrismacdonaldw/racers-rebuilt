#pragma once

#include <string_view>
#include <vector>

#include "app/track_space.hpp"
#include "assets/track_loader.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

namespace rr::app {

struct TrackModel {
    std::vector<render::MeshDraw> draws;
    Bounds bounds;
};

/// `bounds` covers the collidable track surface, or every model when there is none.
[[nodiscard]] Result<TrackModel> UploadTrack(render::Renderer& renderer,
                                             const assets::TrackAssets& track);

/// At the origin, for the caller to move.
[[nodiscard]] Result<std::vector<render::MeshDraw>> UploadModel(render::Renderer& renderer,
                                                                const assets::TrackAssets& assets,
                                                                std::string_view name);

}
