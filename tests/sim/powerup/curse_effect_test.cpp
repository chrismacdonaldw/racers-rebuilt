#include "sim/powerup/curse_effect.hpp"

#include <doctest/doctest.h>

#include "powerup_rig.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;
using test::Rig;

constexpr int kCurseTicks = 1335;
constexpr int kCurseMs = 10'000;
constexpr Vector3 kInField{.x = 12.0F, .z = 0.2F};

TEST_CASE("a curse floats 13 above the ground under its owner") {
    Rig rig;
    rig.Fire(BrickColour::Yellow, 3);

    rig.TickFor(1);

    CHECK(rig.EffectAt() == Vector3{.x = 5.0F, .z = 14.0F});
}

TEST_CASE("a curse lasts 15 s, then fades for 1 s") {
    Rig rig;
    rig.Fire(BrickColour::Yellow, 3);

    CHECK(rig.TicksUntil([&] { return rig.System().Active().empty(); }, {}, kCurseTicks) ==
          kCurseTicks);
}

TEST_CASE("a curse curses every other racer that touches it, once") {
    Rig rig(3);
    rig.Fire(BrickColour::Yellow, 3);
    rig.TickFor(1);
    rig.Place(1, kInField);

    rig.TickFor(1);
    CHECK(rig.Car(1).Status().isCursed);
    CHECK(rig.Car(1).Status().curseMs == kCurseMs - 12);
    CHECK_FALSE(rig.Car(0).Status().isCursed);

    rig.Place(2, kInField);
    rig.TickFor(1);
    CHECK(rig.Car(2).Status().isCursed);
    CHECK(rig.Car(1).Status().curseMs == kCurseMs - 24);
    CHECK(rig.System().Active().size() == 1);
}

TEST_CASE("a shield absorbs a curse without using it up") {
    Rig rig(2);
    rig.Fire(BrickColour::Yellow, 3);
    rig.TickFor(1);
    rig.Place(1, kInField);
    rig.Fire(BrickColour::Blue, 0, 1);

    rig.TickFor(1);

    CHECK_FALSE(rig.Car(1).Status().isCursed);
    CHECK(rig.Car(1).Status().shield.absorbs == 1);
    CHECK(rig.System().Active().size() == 2);
}

}
TEST_CASE("a hazard's curse floats over its aim and curses any racer") {
    Rig rig;
    rig.FireFromHazard(BrickColour::Yellow, 3,
                       {.source = {.x = 12.0F, .z = 1.0F}, .target = {.x = 12.0F, .z = 1.0F}});

    rig.TickFor(2);

    CHECK(rig.EffectAt() == Vector3{.x = 12.0F, .z = 14.0F});
    CHECK(rig.Car().Status().isCursed);
}

}
