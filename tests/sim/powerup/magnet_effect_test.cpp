#include "sim/powerup/magnet_effect.hpp"

#include <doctest/doctest.h>

#include "powerup_rig.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;
using test::Rig;

constexpr int kUntouchedTicks = 1751;
constexpr int kHeldTicks = 168;
constexpr Vector3 kModelHeight{.z = 30.0F};

Vector3 DropMagnet(Rig& rig) {
    rig.Fire(BrickColour::Yellow, 2);
    rig.TickFor(1);
    return rig.EffectAt() - kModelHeight;
}

TEST_CASE("a magnet hangs 30 above the ground under its owner") {
    Rig rig;

    CHECK(DropMagnet(rig) == Vector3{.x = 5.0F, .z = 1.0F});
    CHECK(rig.HasWeaponEvent(WeaponEventKind::Fired));
}

TEST_CASE("an untouched magnet stays armed for 20 s, then fades for 1 s") {
    Rig rig;
    rig.Fire(BrickColour::Yellow, 2);

    CHECK(rig.TicksUntil([&] { return rig.System().Active().empty(); }, {}, kUntouchedTicks) ==
          kUntouchedTicks);
}

void GrabNearby(Rig& rig) {
    const Vector3 drop = DropMagnet(rig);
    rig.Place(1, {.x = drop.x + 5.0F, .y = drop.y, .z = 0.2F});
    rig.TickFor(1);
}

TEST_CASE("a magnet grabs a stopped racer near it and halts it") {
    Rig rig(2);
    GrabNearby(rig);

    CHECK(rig.Car(1).Status().isMagnetHeld);
    CHECK(rig.Car(1).Status().isHalted);
    CHECK(rig.HasWeaponEvent(WeaponEventKind::Struck));
    CHECK(rig.Car(1).Events().reaction == VoiceReaction::Negative);
}

TEST_CASE("a magnet cuts its hold to 1 s once its racer halts, and lets go as it fades") {
    Rig rig(2);
    GrabNearby(rig);

    CHECK(rig.TicksUntil([&] { return rig.System().Active().empty(); }) == kHeldTicks);
    CHECK_FALSE(rig.Car(1).Status().isMagnetHeld);
    CHECK_FALSE(rig.Car(1).Status().isHalted);
}

TEST_CASE("a magnet pulls a racer in its field but out of reach toward the drop point") {
    Rig rig(2);
    const Vector3 drop = DropMagnet(rig);
    rig.Place(1, {.x = drop.x, .y = drop.y + 12.0F, .z = 0.2F});

    rig.TickFor(1);

    CHECK_FALSE(rig.Car(1).Status().isMagnetHeld);
    CHECK(rig.Car(1).Body().motion.velocity.y < 0.0F);
}

TEST_CASE("a shield absorbs a magnet and uses it up") {
    Rig rig(2);
    const Vector3 drop = DropMagnet(rig);
    rig.Place(1, {.x = drop.x + 5.0F, .y = drop.y, .z = 0.2F});
    rig.Fire(BrickColour::Blue, 0, 1);

    rig.TickFor(1);

    CHECK_FALSE(rig.Car(1).Status().isMagnetHeld);
    CHECK(rig.Car(1).Status().shield.absorbs == 1);
    CHECK(rig.System().Active().size() == 1);
}

TEST_CASE("a magnet lets go of a racer that warps away") {
    Rig rig(2);
    GrabNearby(rig);
    REQUIRE(rig.Car(1).Status().isMagnetHeld);

    REQUIRE(rig.Car(1).BeginWarp());
    rig.TickFor(1);

    CHECK_FALSE(rig.Car(1).Status().isMagnetHeld);
}

}
}
