#include "sim/powerup/oil_slick_effect.hpp"

#include <doctest/doctest.h>

#include "powerup_rig.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;
using test::Rig;

constexpr int kSlickTicks = 835;

Vector3 DropSlick(Rig& rig) {
    rig.Fire(BrickColour::Yellow, 0);
    rig.TickFor(1);
    return rig.EffectAt();
}

void PlaceOn(Rig& rig, std::size_t racer, const Vector3& slick) {
    rig.Place(racer, {.x = slick.x, .y = slick.y, .z = 0.2F});
}

TEST_CASE("an oil slick drops under its owner on its first tick") {
    Rig rig;
    rig.Fire(BrickColour::Yellow, 0);
    CHECK(rig.Weapons().empty());

    rig.TickFor(1);

    CHECK(rig.EffectAt() == Vector3{.x = 5.0F, .z = 1.0F});
    CHECK(rig.HasWeaponEvent(WeaponEventKind::Fired));
}

TEST_CASE("an untouched oil slick lasts 10 s") {
    Rig rig;
    rig.Fire(BrickColour::Yellow, 0);

    CHECK(rig.TicksUntil([&] { return rig.System().Active().empty(); }) == kSlickTicks);
}

TEST_CASE("the first racer over an oil slick spins one turn and uses it up") {
    Rig rig(2);
    const Vector3 slick = DropSlick(rig);

    PlaceOn(rig, 1, slick);
    rig.TickFor(1);

    CHECK(rig.Car(1).IsSpinning());
    CHECK(rig.Car(1).Status().hitReactions == 1);
    CHECK(rig.HasWeaponEvent(WeaponEventKind::Struck));
    CHECK(rig.System().Active().empty());
}

TEST_CASE("a racer driving over an oil slick spins") {
    Rig rig(2);
    const Vector3 slick = DropSlick(rig);
    rig.Place(1, {.x = slick.x - 30.0F, .y = slick.y, .z = 0.2F});
    rig.Car(0).Place({.position = {.x = 100.0F, .y = 50.0F, .z = 0.2F}});

    CHECK(rig.TicksUntil([&] { return rig.Car(1).IsSpinning(); }, test::kThrottle) > 0);
}

TEST_CASE("an oil slick never spins its owner") {
    Rig rig;
    DropSlick(rig);
    rig.TickFor(5);

    CHECK_FALSE(rig.Car(0).IsSpinning());
    CHECK(rig.System().Active().size() == 1);
}

TEST_CASE("a halted racer crosses an oil slick untouched") {
    Rig rig(2);
    const Vector3 slick = DropSlick(rig);
    PlaceOn(rig, 1, slick);
    rig.Car(1).Halt();

    rig.TickFor(1);

    CHECK_FALSE(rig.Car(1).IsSpinning());
    CHECK(rig.System().Active().size() == 1);
}

TEST_CASE("a shield absorbs an oil slick and uses it up") {
    Rig rig(2);
    const Vector3 slick = DropSlick(rig);
    PlaceOn(rig, 1, slick);
    rig.Fire(BrickColour::Blue, 0, 1);

    rig.TickFor(1);

    CHECK_FALSE(rig.Car(1).IsSpinning());
    CHECK(rig.Car(1).Status().shield.absorbs == 1);
    CHECK(rig.System().Active().size() == 1);
}

}
}
