#include "sim/collision/collision_mesh.hpp"

#include <doctest/doctest.h>

#include <optional>

#include "test_meshes.hpp"

namespace rr::sim {
namespace {

CollisionMesh FloorAndWall() {
    Result<CollisionMesh> mesh = CollisionMesh::Create(test::FloorAndWall());
    REQUIRE(mesh.has_value());
    return std::move(*mesh);
}

TEST_CASE("a ray down hits the floor on its node's plane") {
    const CollisionMesh mesh = FloorAndWall();

    const std::optional<MeshHit> found = mesh.CastSegment({.z = 5.0F}, {.z = -5.0F});

    REQUIRE(found.has_value());
    const MeshHit& hit = *found;
    CHECK(hit.point == Vector3{});
    CHECK(hit.normal == Vector3{.z = 1.0F});
    CHECK(hit.fraction == 0.5F);
    CHECK(hit.material == 0);
}

TEST_CASE("the nearer wall wins over the floor behind it") {
    const CollisionMesh mesh = FloorAndWall();

    const std::optional<MeshHit> found =
        mesh.CastSegment({.x = 0.0F, .z = 6.0F}, {.x = 10.0F, .z = -2.0F});

    REQUIRE(found.has_value());
    const MeshHit& hit = *found;
    CHECK(hit.point == Vector3{.x = 5.0F, .z = 2.0F});
    CHECK(hit.normal == Vector3{.x = -1.0F});
    CHECK(hit.fraction == 0.5F);
    CHECK(hit.material == 1);
}

TEST_CASE("a wall hit from behind comes back with its own normal") {
    const CollisionMesh mesh = FloorAndWall();

    const std::optional<MeshHit> found =
        mesh.CastSegment({.x = 8.0F, .z = 2.0F}, {.x = 0.0F, .z = 2.0F});

    REQUIRE(found.has_value());
    const MeshHit& hit = *found;
    CHECK(hit.normal == Vector3{.x = -1.0F});
    CHECK(hit.fraction == doctest::Approx(3.0F / 8.0F));
}

TEST_CASE("points on a triangle's edge count as inside") {
    const CollisionMesh mesh = FloorAndWall();

    CHECK(mesh.CastSegment({.x = 10.0F, .y = 3.0F, .z = 1.0F}, {.x = 10.0F, .y = 3.0F, .z = -1.0F})
              .has_value());
    CHECK_FALSE(
        mesh.CastSegment({.x = 10.5F, .y = 3.0F, .z = 1.0F}, {.x = 10.5F, .y = 3.0F, .z = -1.0F})
            .has_value());
}

TEST_CASE("segments that stay on one side or miss the triangles hit nothing") {
    const CollisionMesh mesh = FloorAndWall();

    CHECK_FALSE(mesh.CastSegment({.z = 5.0F}, {.z = 1.0F}).has_value());
    CHECK_FALSE(
        mesh.CastSegment({.x = 20.0F, .y = 20.0F, .z = 5.0F}, {.x = 20.0F, .y = 20.0F, .z = -5.0F})
            .has_value());
}

TEST_CASE("a collision mesh must be a tree no deeper than the query stack") {
    assets::CollisionMeshAsset loop = test::FloorAndWall();
    loop.nodes[1].back = 0;
    assets::CollisionMeshAsset shared = test::FloorAndWall();
    shared.nodes[0].back = 1;

    CHECK_FALSE(CollisionMesh::Create(loop).has_value());
    CHECK_FALSE(CollisionMesh::Create(shared).has_value());
    CHECK(CollisionMesh::Create(test::Chain(kMaxCollisionTreeDepth)).has_value());
    CHECK_FALSE(CollisionMesh::Create(test::Chain(kMaxCollisionTreeDepth + 1)).has_value());
}

TEST_CASE("the deepest allowed tree queries to the bottom") {
    Result<CollisionMesh> mesh = CollisionMesh::Create(test::Chain(kMaxCollisionTreeDepth));
    REQUIRE(mesh.has_value());

    CHECK(mesh->CastSegment({.x = 1.0F, .y = -1.0F, .z = 1.0F}, {.x = 1.0F, .y = -1.0F, .z = -1.0F})
              .has_value());
}

}
}
