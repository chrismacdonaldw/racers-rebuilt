#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "assets/animation_database.hpp"
#include "assets/brick_layout.hpp"
#include "assets/checkpoints.hpp"
#include "assets/collision_mesh.hpp"
#include "assets/event_table.hpp"
#include "assets/hazard_list.hpp"
#include "assets/skeleton_database.hpp"
#include "assets/start_grid.hpp"
#include "assets/surface_table.hpp"
#include "assets/target_points.hpp"
#include "assets/world_database.hpp"

namespace rr::assets {

void MirrorY(WorldAsset& world);
void MirrorY(SkeletonAsset& skeleton);
void MirrorY(AnimationAsset& animations);
void MirrorY(CheckpointsAsset& checkpoints);
void MirrorY(StartGridAsset& grid);
/// Also swaps each triangle's winding, so faces keep facing out.
void MirrorY(CollisionMeshAsset& mesh);
void MirrorY(std::vector<SurfaceEntry>& surfaces);
void MirrorY(BrickLayout& bricks);
void MirrorY(std::vector<TargetPoint>& targets);
void MirrorY(EventTableAsset& table);
void MirrorY(std::vector<Hazard>& hazards);

/// False for the sky world, `BACKGRD.WDB`, which mirrored tracks leave as authored.
[[nodiscard]] bool IsMirroredWorld(std::string_view path);

/// Wraps an asset parser so that, on a mirrored track, what it reads comes back mirrored.
template <typename Parse>
[[nodiscard]] auto MirroredIf(bool isMirrored, Parse parse) {
    return [isMirrored, parse](std::span<const std::byte> bytes) {
        return parse(bytes).transform([isMirrored](auto asset) {
            if (isMirrored) {
                MirrorY(asset);
            }
            return asset;
        });
    };
}

}
