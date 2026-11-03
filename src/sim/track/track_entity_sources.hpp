#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/animation_database.hpp"
#include "assets/skeleton_database.hpp"
#include "assets/vfs.hpp"
#include "assets/world_database.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"

namespace rr::sim {

/// `placement` is in game space, `scale` multiplies skeleton positions, `radius` bounds vertices.
struct TrackEntity {
    std::string name;
    std::string model;
    std::size_t skeleton = 0;
    std::size_t animation = 0;
    std::optional<std::size_t> startPart;
    Transform placement;
    float scale = 1.0F;
    float radius = 0.0F;
    float drawRangeSquared = std::numeric_limits<float>::infinity();
    std::optional<assets::TextureScroll> textureScroll;
    bool isEnvironmentMapped = false;
};

/// `placement` is in game space.
struct StaticEntity {
    std::string name;
    Transform placement;
};

struct TrackEntitySources {
    std::vector<assets::SkeletonAsset> skeletons;
    std::vector<assets::AnimationAsset> animations;
    std::vector<TrackEntity> entities;
    std::vector<StaticEntity> statics;
};

/// Looks files up in the track folder, then in `GAMEDATA/COMMON`.
[[nodiscard]] Result<TrackEntitySources> LoadTrackEntities(const assets::Vfs& vfs,
                                                           std::string_view track,
                                                           bool isMirrored = false);

/// Loads only the named worlds, in order, from `folder`, then from `GAMEDATA/COMMON`.
[[nodiscard]] Result<TrackEntitySources> LoadWorldEntities(const assets::Vfs& vfs,
                                                           std::string_view folder,
                                                           std::span<const std::string> worlds);

}
