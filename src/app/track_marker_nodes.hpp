#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "assets/gltf_scene.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::app {

/// One root per marker kind the track has, including a line per computer racers' route.
[[nodiscard]] Result<std::vector<std::size_t>> AddTrackMarkers(const assets::Vfs& vfs,
                                                               std::string_view track,
                                                               assets::GltfScene& scene);

}
