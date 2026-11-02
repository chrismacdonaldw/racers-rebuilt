#include "sim/hazard/warp_pad.hpp"

#include <doctest/doctest.h>

#include <cmath>

#include "hazard_rig.hpp"

namespace rr::sim {
namespace {

assets::Hazard Pad() {
    return {.kind = assets::HazardKind::WarpPad};
}

}

TEST_CASE("a racer touching the warp pad warps toward its target") {
    test::HazardRig rig({Pad()}, {});

    rig.Hear({.id = kWarpPadEvent, .racer = 0});

    REQUIRE(rig.Powerups().Active().size() == 1);
    CHECK(rig.Effect().kind == PowerupKind::Warp);
    CHECK(rig.Effect().use.racer == 0);
    CHECK(rig.Effect().use.hazard == 0);
    CHECK(rig.Car().Status().isGhost);
    CHECK(rig.Hazards().Aims()[0].target == Vector3{.x = 132.673F, .y = 86.304F, .z = 14.722F});
    CHECK(rig.Hazards().Aims()[0].direction.y == doctest::Approx(-0.5F / std::sqrt(1.25F)));
}

TEST_CASE("the warp pad ignores the position path, ends and ghosts") {
    test::HazardRig rig({Pad()}, {}, 2);
    rig.Car(1).EnterGhostMode();

    rig.Hear({.id = kWarpPadEvent, .position = Vector3{}});
    rig.Hear({.id = kWarpPadEvent, .edge = EventEdge::End, .racer = 0});
    rig.Hear({.id = kWarpPadEvent, .racer = 1});

    CHECK(rig.Powerups().Active().empty());
}

TEST_CASE("a racer already warping isn't warped again") {
    test::HazardRig rig({Pad()}, {});

    rig.Hear({.id = kWarpPadEvent, .racer = 0});
    rig.Hear({.id = kWarpPadEvent, .racer = 0});

    CHECK(rig.Powerups().Active().size() == 1);
}

TEST_CASE("on a mirrored track the warp pad warps toward its mirrored target") {
    test::HazardRig rig({Pad()}, {});
    rig.MirrorTrack();

    rig.Hear({.id = kWarpPadEvent, .racer = 0});

    CHECK(rig.Hazards().Aims()[0].target == Vector3{.x = 132.673F, .y = -86.304F, .z = 14.722F});
    CHECK(rig.Hazards().Aims()[0].direction.y == doctest::Approx(0.5F / std::sqrt(1.25F)));
}

}
