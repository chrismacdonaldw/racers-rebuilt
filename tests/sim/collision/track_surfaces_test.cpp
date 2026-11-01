#include "sim/collision/track_surfaces.hpp"

#include <doctest/doctest.h>

#include <vector>

namespace rr::sim {
namespace {

std::vector<assets::SurfaceEntry> Entries() {
    return {{.name = "road"},
            {.name = "go_thru", .isProjectilePassable = true},
            {.name = "pilcol", .isNonSolid = true, .isProjectilePassable = true}};
}

TEST_CASE("track surfaces start with their table's flags") {
    const TrackSurfaces surfaces(Entries());

    CHECK(surfaces.Count() == 3);
    CHECK(surfaces.IsSolid(0));
    CHECK_FALSE(surfaces.LetsShotsThrough(0));
    CHECK(surfaces.IsSolid(1));
    CHECK(surfaces.LetsShotsThrough(1));
    CHECK_FALSE(surfaces.IsSolid(2));
    CHECK(surfaces.Entry(2).name == "pilcol");
}

TEST_CASE("a surface made passable lets racers and shots through until it blocks again") {
    TrackSurfaces surfaces(Entries());

    surfaces.SetPassable(0, true);
    CHECK_FALSE(surfaces.IsSolid(0));
    CHECK(surfaces.LetsShotsThrough(0));

    surfaces.SetPassable(2, false);
    CHECK(surfaces.IsSolid(2));
    CHECK_FALSE(surfaces.LetsShotsThrough(2));
}

TEST_CASE("track surfaces are found by name") {
    const TrackSurfaces surfaces(Entries());

    CHECK(surfaces.Find("pilcol") == 2U);
    CHECK_FALSE(surfaces.Find("sphinx").has_value());
}

}
}
