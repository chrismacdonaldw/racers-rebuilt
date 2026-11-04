#include <doctest/doctest.h>

#include "powerup_rig.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;
using test::Rig;

constexpr Vector3 kAhead{.x = 205.0F, .z = 0.2F};
constexpr Vector3 kInCurse{.x = 12.0F, .z = 0.2F};

void PlaceOn(Rig& rig, std::size_t racer, const Vector3& trap) {
    rig.Place(racer, {.x = trap.x, .y = trap.y, .z = 0.2F});
}

}

TEST_CASE("an invincible racer shrugs off a cannonball and keeps its white brick") {
    Rig rig(2);
    rig.Place(1, kAhead);
    rig.GiveWhite(1);
    rig.Car(1).MakeInvincible();
    rig.Fire(BrickColour::Red, 0);

    REQUIRE(rig.TicksUntilIdle() > 0);

    CHECK(rig.Car(1).Status().hitReactions == 0);
    CHECK_FALSE(rig.Car(1).IsSpunOut());
    CHECK(rig.Inventory(1).WhiteCount() == 1);
}

TEST_CASE("an invincible racer drives over an oil slick without spinning or using it up") {
    Rig rig(2);
    rig.Fire(BrickColour::Yellow, 0);
    rig.TickFor(1);
    rig.Car(1).MakeInvincible();

    PlaceOn(rig, 1, rig.EffectAt());
    rig.TickFor(1);

    CHECK_FALSE(rig.Car(1).IsSpinning());
    CHECK(rig.Car(1).Status().hitReactions == 0);
    CHECK(rig.System().Active().size() == 1);
}

TEST_CASE("a curse can't curse an invincible racer") {
    Rig rig(2);
    rig.Fire(BrickColour::Yellow, 3);
    rig.TickFor(1);
    rig.Car(1).MakeInvincible();

    rig.Place(1, kInCurse);
    rig.TickFor(1);

    CHECK_FALSE(rig.Car(1).Status().isCursed);
    CHECK(rig.Car(1).Status().hitReactions == 0);
}

TEST_CASE("an invincible racer's shield still absorbs a curse") {
    Rig rig(2);
    rig.Fire(BrickColour::Yellow, 3);
    rig.TickFor(1);
    rig.Car(1).MakeInvincible();
    rig.Place(1, kInCurse);
    rig.Fire(BrickColour::Blue, 0, 1);

    rig.TickFor(1);

    CHECK_FALSE(rig.Car(1).Status().isCursed);
    CHECK(rig.Car(1).Status().shield.absorbs == 1);
}

}
