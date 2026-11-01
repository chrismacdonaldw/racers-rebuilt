#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "assets/animation_database.hpp"
#include "assets/skeleton_database.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"

namespace rr::sim {

/// `placement` is in game space. `scale` multiplies the skeleton's positions.
struct TrackEntity {
    std::string name;
    std::string model;
    std::size_t skeleton = 0;
    std::size_t animation = 0;
    std::optional<std::size_t> startPart;
    Transform placement;
    float scale = 1.0F;
};

struct TrackEntitySources {
    std::vector<assets::SkeletonAsset> skeletons;
    std::vector<assets::AnimationAsset> animations;
    std::vector<TrackEntity> entities;
};

/// Looks files up in the track folder, then in `GAMEDATA/COMMON`.
[[nodiscard]] Result<TrackEntitySources> LoadTrackEntities(const assets::Vfs& vfs,
                                                           std::string_view track);

}
