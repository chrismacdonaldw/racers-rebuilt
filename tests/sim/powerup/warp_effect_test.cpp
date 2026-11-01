#include "sim/powerup/warp_effect.hpp"

#include <doctest/doctest.h>

#include "../ai/straight_route.hpp"
#include "green_rig.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;
using test::kChassis;
using test::Rig;
using test::RouteEffectsOf;

using test::kWarpStartTicks;
using test::kWarpTicks;

TEST_CASE("a warp opens for 500 ms, then makes the racer a ghost") {
    Rig rig;
    rig.CrossFirstGate();
    rig.Fire(BrickColour::Green, 3);

    rig.TickFor(kWarpStartTicks - 1);
    CHECK(rig.Car().Status().isWarping);
    CHECK_FALSE(rig.Car().Status().isGhost);
    rig.Tick({});
    CHECK_FALSE(rig.Car().Status().isWarping);
    CHECK(rig.Car().Status().isGhost);
    CHECK(rig.Car().Events().hasWarpEntered);

    rig.TickFor(kWarpTicks - kWarpStartTicks - 1);
    CHECK(rig.Car().Status().isGhost);
}

TEST_CASE("a warp follows the gates for 1500 ms, then sets the car down facing the next gate") {
    Rig rig;
    rig.CrossFirstGate();
    rig.Fire(BrickColour::Green, 3);

    rig.TickFor(kWarpTicks);

    const BodyMotion& motion = rig.Car().Body().motion;
    CHECK(rig.Car().Events().hasWarpLanded);
    CHECK_FALSE(rig.Car().Status().isGhost);
    CHECK(motion.position.x == doctest::Approx(5.0F + (125 * 7.2F)).epsilon(0.002));
    CHECK(motion.position.z == doctest::Approx(5.0F));
    CHECK(motion.basis.forward.x == doctest::Approx(1.0F));
    CHECK(motion.velocity.x == doctest::Approx(700.0F / kChassis.baseMass));
    CHECK(rig.Progress().OfRacer(0).laps.LastCheckpoint() == 2);
}

TEST_CASE("a ghost or a racer already warping can't warp") {
    Rig rig;
    rig.Car().EnterGhostMode();
    rig.Fire(BrickColour::Green, 3);
    rig.Tick();
    CHECK(rig.System().Active().empty());

    rig.Car().LeaveGhostMode();
    rig.Fire(BrickColour::Green, 3);
    rig.Fire(BrickColour::Green, 3);
    rig.Tick();
    CHECK(rig.System().Active().size() == 1);
}

TEST_CASE("a computer racer warps by playing its route as a ghost") {
    Rig rig;
    rig.Car().FollowRoute(test::StraightRoute({.z = 0.2F}, 3000));
    rig.Fire(BrickColour::Green, 3);
    rig.TickFor(kWarpStartTicks);

    CHECK(rig.Car().Route().transform(&AiRacer::PlaybackSpeed) == 4.5F);
    CHECK(RouteEffectsOf(rig.Car()).isGhost);
    CHECK(rig.TicksUntilIdle() == kWarpTicks - kWarpStartTicks);
    CHECK_FALSE(RouteEffectsOf(rig.Car()).isGhost);
}

}
}
