#pragma once

#include <optional>
#include <string_view>

#include "app/hud/minimap.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// `textures` are the track's uploaded textures. Without bounds the track has no map.
[[nodiscard]] std::optional<TrackMap> LoadTrackMap(
    const assets::Vfs& vfs, std::string_view track, bool isMirrored,
    const assets::NameMap<render::TextureId>& textures);

}
