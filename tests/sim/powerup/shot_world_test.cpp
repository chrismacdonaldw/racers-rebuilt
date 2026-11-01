#include "sim/powerup/shot_world.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <vector>

#include "../physics/plane_world.hpp"

namespace rr::sim {
namespace {

constexpr Vector3 kAbove{.z = 5.0F};
constexpr Vector3 kBelow{.z = -5.0F};
constexpr assets::EventId kShotEvent = 12;

std::vector<assets::SurfaceEntry> Entries() {
    return {{.name = "sphinx", .projectileEvent = kShotEvent},
            {.name = "go_thru", .isProjectilePassable = true}};
}

test::PlaneWorld GroundUnder(std::size_t surface) {
    return test::PlaneWorld(
        {test::TestPlane{},
         test::TestPlane{.offset = 2.0F, .surface = {.tableEntry = surface}, .isSolid = false}});
}

struct Shots {
    TrackSurfaces surfaces{Entries()};
    Random random{1};
    TrackEvents events{TrackEventSources{}, 0, false, random, 4};
};

TEST_CASE("a shot stops at a solid surface and fires its projectile event there") {
    Shots shots;
    const test::PlaneWorld world = GroundUnder(0);

    const std::optional<SegmentHit> hit =
        CastSolid(ShotWorld(world, &shots.surfaces, &shots.events), kAbove, kBelow);

    REQUIRE(hit.has_value());
    const Vector3 at = hit.value_or(SegmentHit{}).point;
    CHECK(at.z == doctest::Approx(2.0F));
    REQUIRE(shots.events.Events().size() == 2);
    CHECK(shots.events.Events()[0] ==
          TrackEvent{.id = kShotEvent, .edge = EventEdge::Start, .position = at});
    CHECK(shots.events.Events()[1] ==
          TrackEvent{.id = kShotEvent, .edge = EventEdge::End, .position = at});
}

TEST_CASE("a surface that lets shots through hides whatever lies behind it") {
    Shots shots;
    const test::PlaneWorld world = GroundUnder(1);

    CHECK_FALSE(CastSolid(ShotWorld(world, &shots.surfaces, &shots.events), kAbove, kBelow));
    CHECK(shots.events.Events().empty());
}

TEST_CASE("a shot passes a surface made passable after firing its event") {
    Shots shots;
    shots.surfaces.SetPassable(0, true);
    const test::PlaneWorld world = GroundUnder(0);

    CHECK_FALSE(CastSolid(ShotWorld(world, &shots.surfaces, &shots.events), kAbove, kBelow));
    CHECK(shots.events.Events().size() == 2);
}

TEST_CASE("without a surface table a shot keeps the nearest hit's own solidity") {
    const test::PlaneWorld world = GroundUnder(0);

    CHECK_FALSE(CastSolid(ShotWorld(world, nullptr, nullptr), kAbove, kBelow).has_value());
}

}
}
