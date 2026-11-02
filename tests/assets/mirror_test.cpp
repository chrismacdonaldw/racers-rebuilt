#include "assets/mirror.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <variant>
#include <vector>

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

TEST_CASE("mirrored checkpoints and start slots reflect their placement") {
    CheckpointsAsset checkpoints{
        .checkpoints = {{.normal = kPoint, .distance = 5.0F, .position = kPoint}}};
    StartGridAsset grid{.slots = {{.position = kPoint, .forward = kPoint, .up = kPoint}}};

    MirrorY(checkpoints);
    MirrorY(grid);

    CHECK(checkpoints.checkpoints[0].normal == kMirroredPoint);
    CHECK(checkpoints.checkpoints[0].distance == 5.0F);
    CHECK(checkpoints.checkpoints[0].position == kMirroredPoint);
    CHECK(grid.slots[0].position == kMirroredPoint);
    CHECK(grid.slots[0].forward == kMirroredPoint);
    CHECK(grid.slots[0].up == kMirroredPoint);
}

TEST_CASE("a mirrored collision mesh reflects its vertices and normals and keeps faces outward") {
    CollisionMeshAsset mesh;
    mesh.vertices = {kPoint};
    mesh.triangles.push_back({.vertices = {1, 2, 3}, .material = 4});
    mesh.nodes.push_back({.normal = kPoint});

    MirrorY(mesh);

    CHECK(mesh.vertices[0] == kMirroredPoint);
    CHECK(mesh.triangles[0].vertices == std::array<std::uint16_t, 3>{1, 3, 2});
    CHECK(mesh.triangles[0].material == 4);
    CHECK(mesh.nodes[0].normal == kMirroredPoint);
}

TEST_CASE("mirrored surfaces reflect their force vectors") {
    std::vector<SurfaceEntry> surfaces(2);
    surfaces[0].vector2C = kPoint;
    surfaces[0].force = kPoint;

    MirrorY(surfaces);

    CHECK(surfaces[0].vector2C == kMirroredPoint);
    CHECK(surfaces[0].force == kMirroredPoint);
    CHECK_FALSE(surfaces[1].force.has_value());
}

TEST_CASE("mirrored bricks and targets reflect their positions") {
    BrickLayout bricks{.coloured = {{.position = kPoint}}, .white = {kPoint}};
    std::vector<TargetPoint> targets{{.position = kPoint}};

    MirrorY(bricks);
    MirrorY(targets);

    CHECK(bricks.coloured[0].position == kMirroredPoint);
    CHECK(bricks.white[0] == kMirroredPoint);
    CHECK(targets[0].position == kMirroredPoint);
}

TEST_CASE("a mirrored event table reflects sounds, particles, look targets and forces") {
    EventTableAsset table;
    table.sounds.push_back({.position = kPoint});
    table.sounds.emplace_back();
    table.particles.push_back({.position = kPoint, .direction = kPoint, .up = kPoint});
    table.lookTargets.push_back({.position = kPoint});
    table.externalForces.push_back({.force = kPoint});

    MirrorY(table);

    CHECK(table.sounds[0].position == kMirroredPoint);
    CHECK_FALSE(table.sounds[1].position.has_value());
    CHECK(table.particles[0].position == kMirroredPoint);
    CHECK(table.particles[0].direction == kMirroredPoint);
    CHECK(table.particles[0].up == kMirroredPoint);
    CHECK(table.lookTargets[0].position == kMirroredPoint);
    CHECK(table.externalForces[0].force == kMirroredPoint);
}

TEST_CASE("mirrored hazards reflect their cannon, ship and lightning positions") {
    std::vector<Hazard> hazards{
        {.payload = CannonShot{.launch = kPoint, .target = kPoint, .triggerCentre = kPoint}},
        {.payload = ShipCannons{.launches = {{.position = kPoint}},
                                .targets = {{.position = kPoint}},
                                .shot = {.launch = kPoint}}},
        {.payload = LightningCannon{.source = kPoint}},
    };

    MirrorY(hazards);

    const auto& shot = std::get<CannonShot>(hazards[0].payload);
    CHECK(shot.launch == kMirroredPoint);
    CHECK(shot.target == kMirroredPoint);
    CHECK(shot.triggerCentre == kMirroredPoint);
    const auto& ship = std::get<ShipCannons>(hazards[1].payload);
    CHECK(ship.launches[0].position == kMirroredPoint);
    CHECK(ship.targets[0].position == kMirroredPoint);
    CHECK(ship.shot.launch == kMirroredPoint);
    CHECK(std::get<LightningCannon>(hazards[2].payload).source == kMirroredPoint);
}

TEST_CASE("every world but the sky world mirrors") {
    CHECK_FALSE(IsMirroredWorld("GAMEDATA/RACEC0R0/BACKGRD.WDB"));
    CHECK_FALSE(IsMirroredWorld("backgrd.wdb"));
    CHECK(IsMirroredWorld("GAMEDATA/RACEC0R0/RRTRK.WDB"));
    CHECK(IsMirroredWorld("GAMEDATA/RACEC0R0/COLLIDE.WDB"));
}

}
}
