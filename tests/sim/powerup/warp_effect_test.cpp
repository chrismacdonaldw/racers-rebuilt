#include "sim/powerup/warp_effect.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <utility>

#include "../ai/straight_route.hpp"
#include "powerup_rig.hpp"

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
TEST_CASE("a warp pad's warp skips the start and moves its z off the target's x by the z gap") {
    Rig rig;
    const Vector3 from = rig.Car().Body().motion.position;
    rig.FireFromHazard(BrickColour::Green, 3,
                       {.source = {.x = 100.0F, .z = 10.0F},
                        .target = {.x = 100.0F, .z = 10.0F},
                        .direction = {.x = 1.0F}});
    CHECK(rig.Car().Status().isGhost);

    rig.TickFor(63, {});

    const float left = 744.0F / 1500.0F;
    CHECK(rig.Car().Body().motion.position.x ==
          doctest::Approx(100.0F + ((from.x - 100.0F) * left)));
    CHECK(rig.Car().Body().motion.position.z ==
          doctest::Approx(100.0F + ((from.z - 10.0F) * left)));
}

TEST_CASE("a warp pad's warp lands at its target and launches along its direction") {
    Rig rig;
    rig.FireFromHazard(BrickColour::Green, 3,
                       {.source = {.x = 100.0F, .z = 10.0F},
                        .target = {.x = 100.0F, .z = 10.0F},
                        .direction = {.y = 1.0F}});

    CHECK(rig.TicksUntil([&] { return !rig.Car().Status().isGhost; }, {}) == 125);

    CHECK(rig.Car().Body().motion.position == Vector3{.x = 100.0F, .z = 5.0F});
    CHECK(rig.Car().Body().motion.velocity.x == doctest::Approx(0.0F));
    CHECK(rig.Car().Body().motion.velocity.y > 0.0F);
}

namespace {

constexpr WarpRules kFixedWarp{.endsWhenTakenBack = true};

void FillWarpPool(Rig& rig) {
    rig.CrossFirstGate();
    rig.Fire(BrickColour::Green, 3, 0);
    rig.Tick({});
    rig.Fire(BrickColour::Green, 3, 1);
    rig.Fire(BrickColour::Green, 3, 2);
}

void TakeBackFirstWarp(Rig& rig) {
    rig.Fire(BrickColour::Green, 3, 3);
    REQUIRE(rig.System().Active().size() == 3);
}

}

TEST_CASE("a warp taken back mid-flight leaves its racer a ghost, as the original does") {
    Rig rig(4);
    rig.Car(0).FollowRoute(test::StraightRoute({.z = 0.2F}, 3000));
    FillWarpPool(rig);
    rig.TickFor(kWarpStartTicks, {});
    TakeBackFirstWarp(rig);

    rig.TickFor(kWarpTicks, {});

    CHECK(rig.Car(0).Status().isGhost);
    CHECK(rig.Car(0).Route().transform(&AiRacer::PlaybackSpeed) == 4.5F);
    rig.Fire(BrickColour::Green, 3, 0);
    rig.Tick({});
    CHECK(rig.System().Active().empty());
}

TEST_CASE("the stuck warp fix lands a warp taken back mid-flight") {
    Rig rig(4, kFixedWarp);
    rig.Car(0).FollowRoute(test::StraightRoute({.z = 0.2F}, 3000));
    FillWarpPool(rig);
    rig.TickFor(kWarpStartTicks, {});
    TakeBackFirstWarp(rig);

    CHECK_FALSE(rig.Car(0).Status().isGhost);
    CHECK(rig.Car(0).Route().transform(&AiRacer::PlaybackSpeed) == 1.0F);
}

TEST_CASE("a warp taken back while opening leaves its racer warping, as the original does") {
    Rig rig(4);
    FillWarpPool(rig);
    TakeBackFirstWarp(rig);

    rig.TickFor(kWarpTicks, {});

    CHECK(rig.Car(0).Status().isWarping);
    CHECK_FALSE(rig.Car(0).Status().isGhost);
}

TEST_CASE("the stuck warp fix cancels a warp taken back while opening") {
    Rig rig(4, kFixedWarp);
    FillWarpPool(rig);
    TakeBackFirstWarp(rig);

    CHECK_FALSE(rig.Car(0).Status().isWarping);
}

namespace {

BodyMotion LandAfterWarp(const WarpRules& rules, const test::PlaneWorld& world) {
    Rig rig(1, rules, world);
    rig.CrossFirstGate();
    rig.Fire(BrickColour::Green, 3);
    rig.TickFor(kWarpTicks, {});
    REQUIRE(rig.Car().Events().hasWarpLanded);
    return rig.Car().Body().motion;
}

}

TEST_CASE("Better Warp lands facing away from a wall the gates point into") {
    const test::PlaneWorld world(
        {test::TestPlane{}, test::TestPlane{.normal = {.x = -1.0F}, .offset = -940.0F}});

    CHECK(LandAfterWarp({}, world).basis.forward.x == doctest::Approx(1.0F));
    const BodyMotion better = LandAfterWarp({.aimsLanding = true}, world);
    const Vector3 turned = better.basis.forward;
    CHECK(turned.x == doctest::Approx(0.5F));
    CHECK(turned.y == doctest::Approx(0.866F).epsilon(0.001));
    CHECK(better.velocity.y > 0.0F);
}

TEST_CASE("Better Warp lands a track without checkpoints facing +X") {
    Result<CheckpointGraph> empty = CheckpointGraph::Create({});
    REQUIRE(empty.has_value());
    Rig rig(1, {.aimsLanding = true}, test::FlatGround(), std::move(*empty));
    rig.Fire(BrickColour::Green, 3);

    rig.TickFor(kWarpTicks, {});

    REQUIRE(rig.Car().Events().hasWarpLanded);
    CHECK(rig.Car().Body().motion.basis.forward.x == doctest::Approx(1.0F));
}

TEST_CASE("a fair warp flies a shorter way and lands at top speed") {
    Rig rig(1, {.isFair = true});
    rig.CrossFirstGate();
    rig.Fire(BrickColour::Green, 3);

    rig.TickFor(kWarpTicks);

    const BodyMotion& motion = rig.Car().Body().motion;
    REQUIRE(rig.Car().Events().hasWarpLanded);
    CHECK(motion.position.x == doctest::Approx(5.0F + (125 * 12 * 0.36F)).epsilon(0.002));
    CHECK(motion.velocity.x == doctest::Approx(rig.Car().Body().maxSpeed / 1000.0F));
}

TEST_CASE("a fair warp plays a computer racer's route at 3") {
    Rig rig(1, {.isFair = true});
    rig.Car().FollowRoute(test::StraightRoute({.z = 0.2F}, 3000));
    rig.Fire(BrickColour::Green, 3);
    rig.TickFor(kWarpStartTicks);

    CHECK(rig.Car().Route().transform(&AiRacer::PlaybackSpeed) == 3.0F);
}

TEST_CASE("a catch-up warp slows only the race leader's warp") {
    Rig rig(2, {.catchesUp = true});
    for (std::size_t racer = 0; racer < 2; ++racer) {
        rig.Car(racer).FollowRoute(test::StraightRoute({.y = 20.0F * static_cast<float>(racer)}));
    }
    REQUIRE(rig.Progress().Standings().front() == 0);
    rig.Fire(BrickColour::Green, 3, 0);
    rig.Fire(BrickColour::Green, 3, 1);
    rig.TickFor(kWarpStartTicks);

    CHECK(rig.Car(0).Route().transform(&AiRacer::PlaybackSpeed).value_or(0.0F) ==
          doctest::Approx(1.0F + (3.5F / 3.0F)));
    CHECK(rig.Car(1).Route().transform(&AiRacer::PlaybackSpeed) == 4.5F);
}

}
