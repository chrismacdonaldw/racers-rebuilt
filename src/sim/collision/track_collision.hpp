#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "assets/surface_table.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "sim/collision/collision_world.hpp"

namespace rr::sim {

struct NamedWorld {
    std::string name;
    CollisionWorld world;
};

/// One world per bounded entity in `COLLIDE.WDB`, with materials bound to the track's surface
/// table.
struct TrackCollision {
    std::vector<NamedWorld> worlds;
    std::size_t track = 0;
    std::optional<std::size_t> checkpoints;
    std::vector<assets::SurfaceEntry> surfaces;

    [[nodiscard]] const CollisionWorld& Track() const { return worlds.at(track).world; }
};

/// Whether a `COLLIDE.WDB` entity is the track itself, which stays where its volume puts it.
[[nodiscard]] bool IsTrackCollisionWorld(std::string_view name);

/// `folder` is a race track folder under `GAMEDATA`, such as `RACEC0R0`.
[[nodiscard]] Result<TrackCollision> LoadTrackCollision(const assets::Vfs& vfs,
                                                        std::string_view folder,
                                                        bool isMirrored = false);

}
