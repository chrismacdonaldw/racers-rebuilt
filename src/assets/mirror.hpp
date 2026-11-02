#pragma once

#include "assets/animation_database.hpp"
#include "assets/skeleton_database.hpp"
#include "assets/world_database.hpp"

namespace rr::assets {

void MirrorY(WorldAsset& world);
void MirrorY(SkeletonAsset& skeleton);
void MirrorY(AnimationAsset& animations);

}
