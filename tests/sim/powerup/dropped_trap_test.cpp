#include "sim/powerup/dropped_trap.hpp"

#include <doctest/doctest.h>

#include "powerup_rig.hpp"

namespace rr::sim {
namespace {

TEST_CASE("a trap drops 1 above the ground under its owner") {
    const test::PlaneWorld world = test::FlatGround();
    const Racer owner(test::kChassis, {}, {.position = {.x = 3.0F, .y = 4.0F, .z = 0.2F}});

    const DropPoint drop = DropUnder(owner, world);

    CHECK(drop.position == Vector3{.x = 3.0F, .y = 4.0F, .z = 1.0F});
    CHECK(drop.normal == Vector3{.z = 1.0F});
}

TEST_CASE("a trap with no ground within 50 below drops at the car") {
    const test::PlaneWorld world = test::FlatGround();
    const Racer owner(test::kChassis, {}, {.position = {.z = 60.0F}});

    CHECK(DropUnder(owner, world).position == Vector3{.z = 60.0F});
}

TEST_CASE("a trap timer runs out once the time left fits in a tick") {
    Milliseconds remaining{20};

    CHECK_FALSE(RunsOut(remaining, Milliseconds{12}));
    CHECK(remaining == Milliseconds{8});
    CHECK(RunsOut(remaining, Milliseconds{12}));
    CHECK(remaining == Milliseconds{0});
}

}
}
