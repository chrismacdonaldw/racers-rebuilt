#include "assets/mirror.hpp"

#include <span>
#include <vector>

#include "core/mirror.hpp"

namespace rr::assets {
namespace {

void MirrorEntities(std::span<WorldEntity> entities) {
    for (WorldEntity& entity : entities) {
        entity.position = MirroredY(entity.position);
        entity.forward = MirroredY(entity.forward);
        entity.up = MirroredY(entity.up);
    }
}

}

void MirrorY(WorldAsset& world) {
    MirrorEntities(world.modelEntities);
    MirrorEntities(world.animatedEntities);
    MirrorEntities(world.collidableEntities);
    MirrorEntities(world.boundedEntities);
    for (WorldSprite& sprite : world.sprites) {
        sprite.position = MirroredY(sprite.position);
    }
}

void MirrorY(SkeletonAsset& skeleton) {
    for (SkeletonNode& node : skeleton.nodes) {
        node.position = MirroredY(node.position);
        node.rotation = MirroredY(node.rotation);
    }
}

void MirrorY(AnimationAsset& animations) {
    for (Vector3& key : animations.positionKeys) {
        key = MirroredY(key);
    }
    for (Quaternion& key : animations.rotationKeys) {
        key = MirroredY(key);
    }
    for (AnimationPart& part : animations.parts) {
        part.velocity = MirroredY(part.velocity);
        part.bounds.centre = MirroredY(part.bounds.centre);
    }
}

}
