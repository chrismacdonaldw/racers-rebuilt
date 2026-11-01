#include "sim/powerup/grapple_effect.hpp"

#include <doctest/doctest.h>

#include <algorithm>

#include "powerup_rig.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;
using test::Rig;

constexpr Vector3 kAhead{.x = 105.0F, .z = 0.2F};

bool HasWeaponEvent(const Rig& rig, WeaponEventKind kind) {
    return std::ranges::any_of(rig.Weapons(),
                               [kind](const WeaponEvent& event) { return event.kind == kind; });
}

bool IsPulled(Rig& rig, std::size_t racer) {
    return std::ranges::any_of(rig.Car(racer).Body().externalForces,
                               [](const auto& force) { return force.has_value(); });
}

void HookRacerAhead(Rig& rig) {
    rig.Place(1, kAhead);
    rig.GiveWhite(1);
    rig.Fire(BrickColour::Red, 1);
    REQUIRE(rig.TicksUntil([&] { return HasWeaponEvent(rig, WeaponEventKind::Struck); }) > 0);
}

TEST_CASE("red level 1 fires a grappling hook") {
    Rig rig;
    rig.Fire(BrickColour::Red, 1);

    REQUIRE(rig.Weapons().size() == 1);
    CHECK(rig.Weapons()[0].kind == WeaponEventKind::Fired);
    CHECK(rig.Weapons()[0].weapon == PowerupKind::GrapplingHook);
}

TEST_CASE("a grappling hook that hits a racer pulls the two cars together") {
    Rig rig(2);
    HookRacerAhead(rig);

    CHECK(rig.Car(1).Status().hitReactions == 1);
    CHECK(rig.Inventory(1).WhiteCount() == 0);
    CHECK(rig.Car(0).Body().externalForces[1].value_or(Vector3{}).x > 0.0F);
    CHECK(rig.Car(1).Body().externalForces[0].value_or(Vector3{}).x < 0.0F);
}

TEST_CASE("a grappling hook lets go once the cars close in and reels back") {
    Rig rig(2);
    HookRacerAhead(rig);
    const float gap = rig.Car(1).Body().motion.position.x - rig.Car(0).Body().motion.position.x;

    REQUIRE(rig.TicksUntil([&] { return HasWeaponEvent(rig, WeaponEventKind::Ended); }) > 0);

    const Vector3 between = rig.Car(1).Body().motion.position - rig.Car(0).Body().motion.position;
    CHECK(Length(between) < gap);
    CHECK_FALSE(IsPulled(rig, 0));
    CHECK_FALSE(IsPulled(rig, 1));
    CHECK(rig.TicksUntil([&] { return rig.System().Active().empty(); }) > 0);
}

TEST_CASE("a grappling hook with nothing ahead misses and reels back") {
    Rig rig;
    rig.Fire(BrickColour::Red, 1);

    REQUIRE(rig.TicksUntil([&] { return HasWeaponEvent(rig, WeaponEventKind::Landed); }) > 0);
    CHECK(rig.TicksUntil([&] { return rig.System().Active().empty(); }, {}, 42) > 0);
}

TEST_CASE("a shield stops a grappling hook from taking hold") {
    Rig rig(2);
    rig.Place(1, kAhead);
    rig.Fire(BrickColour::Blue, 0, 1);
    rig.Fire(BrickColour::Red, 1);

    REQUIRE(rig.TicksUntil([&] { return HasWeaponEvent(rig, WeaponEventKind::Ended); }) > 0);

    CHECK(rig.Car(1).Status().shield.absorbs == 1);
    CHECK(rig.Car(1).Status().hitReactions == 0);
    CHECK_FALSE(IsPulled(rig, 0));
}

TEST_CASE("a player's grappling hook aims at a target point when no racer is ahead") {
    Rig rig;
    rig.AimAt({{.position = {.x = 205.0F, .y = 20.0F}}});
    rig.Fire(BrickColour::Red, 1);

    REQUIRE(rig.TicksUntil([&] { return HasWeaponEvent(rig, WeaponEventKind::Landed); }) > 0);
    const Vector3 landed = rig.Weapons()[0].position;
    CHECK(landed.x == doctest::Approx(205.0F).epsilon(0.02));
    CHECK(landed.y == doctest::Approx(20.0F).epsilon(0.05));
}

}
}
