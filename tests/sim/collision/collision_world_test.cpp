#include "sim/collision/collision_world.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <optional>
#include <vector>

#include "sim/collision/world_list.hpp"
#include "sim/physics/car_body.hpp"
#include "test_meshes.hpp"

namespace rr::sim {
namespace {

CollisionMesh Mesh(std::vector<std::string> materials = {"road", "wall"}) {
    Result<CollisionMesh> mesh = CollisionMesh::Create(test::FloorAndWall(std::move(materials)));
    REQUIRE(mesh.has_value());
    return std::move(*mesh);
}

CollisionWorld Track() {
    return CollisionWorld::Track(Mesh(), {});
}

std::vector<assets::SurfaceEntry> Surfaces() {
    return {{.name = "water",
             .force = Vector3{.y = 50.0F},
             .friction = 0.15F,
             .rollingResistance = 26.0F},
            {.name = "in", .isNonSolid = true}};
}

TEST_CASE("materials bind to their surface entry or the road defaults") {
    const assets::CollisionMeshAsset mesh = test::FloorAndWall({"road", "water", "in", "3"});

    const std::vector<MaterialBinding> track = BindMaterials(mesh, Surfaces(), false);
    const std::vector<MaterialBinding> checkpoints = BindMaterials(mesh, Surfaces(), true);

    REQUIRE(track.size() == 4);
    CHECK(track[0].surface == Surface{});
    CHECK(track[0].isSolid);
    CHECK(track[1].surface.rollingResistance == 26.0F);
    CHECK(track[1].surface.friction == 0.15F);
    CHECK(track[1].surface.lateralGrip == Surface{}.lateralGrip);
    CHECK(track[1].surface.force == Vector3{.y = 50.0F});
    CHECK_FALSE(track[2].isSolid);
    CHECK(track[3].isSolid);
    CHECK_FALSE(checkpoints[3].isSolid);
}

TEST_CASE("a track world hit carries its material in world space") {
    const CollisionMesh mesh = Mesh({"road", "in"});
    const std::vector<MaterialBinding> materials = BindMaterials(mesh.Asset(), Surfaces(), false);
    const CollisionWorld world = CollisionWorld::Track(Mesh({"road", "in"}), materials);

    const std::optional<SegmentHit> found =
        world.CastSegment({.x = 0.0F, .z = 6.0F}, {.x = 10.0F, .z = -2.0F});

    REQUIRE(found.has_value());
    const SegmentHit& hit = *found;
    CHECK(hit.point == Vector3{.x = 5.0F, .z = 2.0F});
    CHECK_FALSE(hit.isSolid);
    CHECK(hit.isTrackWorld);
}

TEST_CASE("a placed world is queried in its own space") {
    const Placement turned{
        .position = {.x = 100.0F},
        .basis = {.forward = {.y = 1.0F}, .side = {.x = -1.0F}, .up = {.z = 1.0F}}};
    const CollisionWorld world = CollisionWorld::Placed(Mesh(), {}, turned);

    const std::optional<SegmentHit> found =
        world.CastSegment({.x = 100.0F, .z = 6.0F}, {.x = 100.0F, .y = 10.0F, .z = -2.0F});

    REQUIRE(found.has_value());
    const SegmentHit& hit = *found;
    CHECK(hit.point.x == doctest::Approx(100.0F));
    CHECK(hit.point.y == doctest::Approx(5.0F));
    CHECK(hit.normal.y == doctest::Approx(-1.0F));
    CHECK_FALSE(hit.isTrackWorld);
}

std::vector<float> FloorHeights(const WorldList& list) {
    std::vector<float> heights;
    for (std::size_t world = 0; world < list.WorldCount(); ++world) {
        const std::optional<SegmentHit> hit = list.CastSegment(world, {.z = 5.0F}, {.z = -5.0F});
        heights.push_back(hit ? hit->point.z : -1.0F);
    }
    return heights;
}

TEST_CASE("a world list tests its extra worlds in slot order, then the track") {
    const CollisionWorld track = Track();
    const CollisionWorld gate = CollisionWorld::Placed(Mesh(), {}, {.position = {.z = 1.0F}});
    const CollisionWorld door = CollisionWorld::Placed(Mesh(), {}, {.position = {.z = 2.0F}});
    WorldList list(track);

    CHECK(list.Add(gate));
    CHECK(list.Add(door));
    CHECK(list.Add(gate));

    CHECK(FloorHeights(list) == std::vector{1.0F, 2.0F, 0.0F});
    list.Remove(gate);
    CHECK(FloorHeights(list) == std::vector{2.0F, 0.0F});
}

TEST_CASE("a world list holds the track and four more worlds") {
    const CollisionWorld track = Track();
    std::vector<CollisionWorld> extras;
    extras.reserve(5);
    for (int i = 0; i < 5; ++i) {
        extras.push_back(CollisionWorld::Placed(Mesh(), {}, {}));
    }
    WorldList list(track);

    for (int i = 0; i < 4; ++i) {
        CHECK(list.Add(extras[static_cast<std::size_t>(i)]));
    }
    CHECK_FALSE(list.Add(extras[4]));
    CHECK(list.WorldCount() == kMaxRacerWorlds);
}

TEST_CASE("a car drives through a gate world and is stopped by the track wall") {
    const assets::CollisionMeshAsset trackMesh =
        test::FloorAndWall({"road", "wall"}, 200.0F, 60.0F);
    const assets::CollisionMeshAsset gateMesh = test::FloorAndWall({"road", "in"}, 200.0F, 30.0F);
    const std::vector<MaterialBinding> gateMaterials = BindMaterials(gateMesh, Surfaces(), false);
    const CollisionWorld track =
        CollisionWorld::Track(CollisionMesh::Create(trackMesh).value(), {});
    const CollisionWorld gate =
        CollisionWorld::Placed(CollisionMesh::Create(gateMesh).value(), gateMaterials, {});
    WorldList worlds(track);
    REQUIRE(worlds.Add(gate));
    const BodyConstants constants = MakeBodyConstants({}, {});
    BodyState body = MakeBody(constants, {.z = 0.2F}, {});
    SetDrive(body, constants, 54.0F, 120.0F);
    float furthestX = 0.0F;

    for (int elapsedMs = 0; elapsedMs < 6000; elapsedMs += 12) {
        StepBody(body, constants, 12, worlds);
        furthestX = std::max(furthestX, body.motion.position.x);
    }

    CHECK(furthestX > 40.0F);
    CHECK(furthestX < 56.0F);
}

}
}
