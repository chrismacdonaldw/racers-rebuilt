#include "assets/mirror.hpp"

#include <doctest/doctest.h>

#include <cmath>

#include "core/mirror.hpp"

namespace rr::assets {
namespace {

constexpr Vector3 kPoint{.x = 1.0F, .y = 2.0F, .z = 3.0F};
constexpr Vector3 kMirroredPoint{.x = 1.0F, .y = -2.0F, .z = 3.0F};

Quaternion Turn(const Vector3& axis, float radians) {
    const float s = std::sin(radians / 2.0F);
    return {.w = std::cos(radians / 2.0F), .x = axis.x * s, .y = axis.y * s, .z = axis.z * s};
}

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x));
    CHECK(actual.y == doctest::Approx(expected.y));
    CHECK(actual.z == doctest::Approx(expected.z));
}

TEST_CASE("a mirrored rotation turns mirrored vectors the way the original turns them") {
    const Quaternion rotation =
        Turn({.x = 0.6F, .y = 0.0F, .z = 0.8F}, 0.7F) * Turn({.y = 1.0F}, 0.4F);

    CheckNear(Rotate(MirroredY(rotation), MirroredY(kPoint)), MirroredY(Rotate(rotation, kPoint)));
    CHECK(MirroredY(MirroredY(rotation)) == rotation);
}

TEST_CASE("a mirrored world reflects its entities and sprites but keeps its cameras") {
    WorldAsset world;
    world.modelEntities.push_back({.position = kPoint, .forward = {.y = 1.0F}});
    world.animatedEntities.push_back({.position = kPoint});
    world.collidableEntities.push_back({.up = {.y = 1.0F}});
    world.boundedEntities.push_back({.position = kPoint});
    world.sprites.push_back({.position = kPoint});
    world.cameras.push_back({.position = kPoint});

    MirrorY(world);

    CHECK(world.modelEntities[0].position == kMirroredPoint);
    CHECK(world.modelEntities[0].forward == Vector3{.y = -1.0F});
    CHECK(world.modelEntities[0].up == Vector3{.z = 1.0F});
    CHECK(world.animatedEntities[0].position == kMirroredPoint);
    CHECK(world.collidableEntities[0].up == Vector3{.y = -1.0F});
    CHECK(world.boundedEntities[0].position == kMirroredPoint);
    CHECK(world.sprites[0].position == kMirroredPoint);
    CHECK(world.cameras[0].position == kPoint);
}

TEST_CASE("a mirrored skeleton reflects each node's placement") {
    const Quaternion rotation = Turn({.z = 1.0F}, 0.5F);
    SkeletonAsset skeleton;
    skeleton.nodes.push_back({.position = kPoint, .rotation = rotation});

    MirrorY(skeleton);

    CHECK(skeleton.nodes[0].position == kMirroredPoint);
    CHECK(skeleton.nodes[0].rotation == MirroredY(rotation));
}

TEST_CASE("mirrored animations reflect their keys, part velocities and bounds") {
    const Quaternion rotation = Turn({.x = 1.0F}, 0.5F);
    AnimationAsset animations;
    animations.positionKeys.push_back(kPoint);
    animations.rotationKeys.push_back(rotation);
    animations.parts.push_back({.velocity = kPoint, .bounds = {.centre = kPoint, .radius = 4.0F}});

    MirrorY(animations);

    CHECK(animations.positionKeys[0] == kMirroredPoint);
    CHECK(animations.rotationKeys[0] == MirroredY(rotation));
    CHECK(animations.parts[0].velocity == kMirroredPoint);
    CHECK(animations.parts[0].bounds.centre == kMirroredPoint);
    CHECK(animations.parts[0].bounds.radius == 4.0F);
}

}
}
