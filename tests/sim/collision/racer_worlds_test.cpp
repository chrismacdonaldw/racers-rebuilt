#include "sim/collision/racer_worlds.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../physics/plane_world.hpp"
#include "test_meshes.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr Vector3 kAbove{.x = 1.0F, .z = 5.0F};
constexpr Vector3 kBelow{.x = 1.0F, .z = -5.0F};

std::vector<assets::SurfaceEntry> Entries() {
    return {{.name = "door"}, {.name = "in", .isNonSolid = true}};
}

NamedWorld Floor(std::string name, std::string material) {
    Result<CollisionMesh> mesh =
        CollisionMesh::Create(test::FloorAndWall({std::move(material), "wall"}));
    REQUIRE(mesh.has_value());
    std::vector<MaterialBinding> bindings = BindMaterials(mesh->Asset(), Entries(), false);
    return {.name = std::move(name),
            .world = CollisionWorld::Track(std::move(*mesh), std::move(bindings))};
}

std::optional<std::size_t> EntryAt(const CollisionQuery& query, std::size_t world) {
    const std::optional<SegmentHit> hit = query.CastSegment(world, kAbove, kBelow);
    return hit ? hit->surface.tableEntry : std::nullopt;
}

bool IsSolidAt(const CollisionQuery& query, std::size_t world) {
    const std::optional<SegmentHit> hit = query.CastSegment(world, kAbove, kBelow);
    return hit && hit->isSolid;
}

TrackCollision Collision() {
    TrackCollision collision;
    collision.worlds.push_back(Floor("collide", "road"));
    collision.worlds.push_back(Floor("chckpt00", "in"));
    collision.worlds.push_back(Floor("doorcol", "door"));
    collision.checkpoints = 1;
    collision.surfaces = Entries();
    return collision;
}

TEST_CASE("a switched-on trigger world stays for 150 ms after its last touch") {
    TriggerWorlds worlds;
    worlds.SwitchOn(2);

    for (int tick = 0; tick < 12; ++tick) {
        worlds.Tick(kTick);
    }
    CHECK(worlds.Worlds().size() == 1);
    worlds.SwitchOn(2);
    for (int tick = 0; tick < 12; ++tick) {
        worlds.Tick(kTick);
    }
    CHECK(worlds.Worlds().size() == 1);
    worlds.Tick(kTick);
    CHECK(worlds.Worlds().empty());
}

TEST_CASE("a racer holds at most three trigger worlds and drops each as it expires") {
    TriggerWorlds worlds;
    worlds.SwitchOn(4);
    worlds.Tick(Milliseconds{100});
    worlds.SwitchOn(5);
    worlds.SwitchOn(6);
    worlds.SwitchOn(7);

    REQUIRE(worlds.Worlds().size() == 3);
    worlds.Tick(Milliseconds{60});
    REQUIRE(worlds.Worlds().size() == 2);
    CHECK(worlds.Worlds()[0].world == 5);
    CHECK(worlds.Worlds()[1].world == 6);
}

TEST_CASE("a racer tests the checkpoint world and its trigger worlds before the track") {
    const TrackCollision collision = Collision();
    const test::PlaneWorld base = test::FlatGround();
    TriggerWorlds switched;
    switched.SwitchOn(2);
    const TrackSurfaces surfaces(collision.surfaces);
    const RacerWorldView view(base, &collision, switched, surfaces);

    REQUIRE(view.WorldCount() == 3);
    CHECK(EntryAt(view, 0) == 1U);
    CHECK(EntryAt(view, 1) == 0U);
    CHECK_FALSE(EntryAt(view, 2).has_value());
}

TEST_CASE("a racer's hits on a table surface follow the surface's current solidity") {
    const TrackCollision collision = Collision();
    const test::PlaneWorld base = test::FlatGround();
    TriggerWorlds switched;
    switched.SwitchOn(2);
    TrackSurfaces surfaces(collision.surfaces);

    CHECK_FALSE(IsSolidAt(RacerWorldView(base, &collision, switched, surfaces), 0));
    CHECK(IsSolidAt(RacerWorldView(base, &collision, switched, surfaces), 1));
    surfaces.SetPassable(0, true);
    CHECK_FALSE(IsSolidAt(RacerWorldView(base, &collision, switched, surfaces), 1));
}

TEST_CASE("without track collision a racer tests only its base world") {
    const test::PlaneWorld base = test::FlatGround();
    const TriggerWorlds switched;
    const TrackSurfaces surfaces;

    CHECK(RacerWorldView(base, nullptr, switched, surfaces).WorldCount() == 1);
}

}
}
