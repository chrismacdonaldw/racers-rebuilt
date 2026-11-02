#pragma once

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

}
