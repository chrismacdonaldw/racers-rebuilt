#include "sim/powerup/missile_effect.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>

#include "powerup_rig.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;
using test::Rig;

constexpr int kArmTicks = 78;

std::optional<Vector3> MissileAt(const Rig& rig, std::uint8_t shot) {
    for (const std::uint16_t slot : rig.System().Active()) {
        const EffectSlot& effect = rig.System().Slots()[slot];
        if (effect.kind == PowerupKind::Missile && effect.use.shot == shot) {
            return effect.effect->Pose().position;
        }
    }
    return std::nullopt;
}

float SideAtGate(Rig& rig, std::uint8_t shot) {
    rig.TicksUntil([&] { return MissileAt(rig, shot).value_or(Vector3{}).x > 400.0F; });
    return MissileAt(rig, shot).value_or(Vector3{}).y;
}

TEST_CASE("red level 3 fires three missiles") {
    Rig rig;
    rig.Fire(BrickColour::Red, 3);

    CHECK(rig.System().Active().size() == 3);
    CHECK(rig.Weapons().size() == 3);
}

TEST_CASE("each missile rides on the car until its launch part ends") {
    Rig rig;
    rig.Fire(BrickColour::Red, 3);

    rig.TickFor(kArmTicks - 1);
    CHECK(MissileAt(rig, 0) == rig.Car().Pose().position);
    rig.Tick({});
    CHECK(MissileAt(rig, 0).value_or(Vector3{}).x > rig.Car().Pose().position.x);
    CHECK(MissileAt(rig, 1) == rig.Car().Pose().position);
}

TEST_CASE("missile i homes on the (i + 1)th racer ahead") {
    Rig rig(4);
    rig.Place(1, {.x = 105.0F, .y = 20.0F, .z = 0.2F});
    rig.Place(2, {.x = 125.0F, .y = -20.0F, .z = 0.2F});
    rig.Place(3, {.x = 145.0F, .z = 0.2F});
    rig.Fire(BrickColour::Red, 3);

    REQUIRE(rig.TicksUntil([&] { return rig.System().Active().empty(); }) > 0);

    for (std::size_t racer = 1; racer < 4; ++racer) {
        CAPTURE(racer);
        CHECK(rig.Car(racer).Status().hitReactions == 1);
    }
    CHECK(rig.Car(0).Status().hitReactions == 0);
}

TEST_CASE("a missile spins the racer it hits two turns") {
    Rig rig(2);
    rig.Place(1, {.x = 155.0F, .z = 0.2F});
    rig.Fire(BrickColour::Red, 3);

    REQUIRE(rig.TicksUntil([&] { return rig.Car(1).Status().hitReactions > 0; }) > 0);

    CHECK(rig.Car(1).IsSpinning());
}

TEST_CASE("with no target the outer missiles fan out to the right and left") {
    Rig rig;
    rig.Fire(BrickColour::Red, 3);
    rig.TickFor(kArmTicks + 120);

    CHECK(MissileAt(rig, 0).value_or(Vector3{}).y < -20.0F);
    CHECK(MissileAt(rig, 2).value_or(Vector3{}).y > 20.0F);
}

TEST_CASE("with no target a missile follows the gates ahead of its owner") {
    Rig loose;
    loose.Fire(BrickColour::Red, 3);
    Rig guided;
    guided.CrossFirstGate();
    guided.Fire(BrickColour::Red, 3);

    const float looseSide = SideAtGate(loose, 0);
    const float guidedSide = SideAtGate(guided, 0);

    CHECK(looseSide < -60.0F);
    CHECK(guidedSide > looseSide + 30.0F);
}

TEST_CASE("a missile hugs the ground and runs out after 5,500 ms") {
    Rig rig;
    rig.Fire(BrickColour::Red, 3);
    rig.TickFor(kArmTicks + 100);
    CHECK(MissileAt(rig, 1).value_or(Vector3{}).z == doctest::Approx(6.0F).epsilon(0.7));

    Rig timed;
    timed.Fire(BrickColour::Red, 3);
    CHECK(timed.TicksUntil([&] { return !MissileAt(timed, 1).has_value(); }) == 558);
}

}
}
