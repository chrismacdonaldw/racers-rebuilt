#include "sim/powerup/cannonball_effect.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>

#include "powerup_rig.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;
using test::kChassis;
using test::Rig;

constexpr Vector3 kAhead{.x = 205.0F, .z = 0.2F};

bool HasWeaponEvent(const Rig& rig, WeaponEventKind kind) {
    return std::ranges::any_of(rig.Weapons(),
                               [kind](const WeaponEvent& event) { return event.kind == kind; });
}

bool IsExploding(const Rig& rig) {
    return std::ranges::any_of(rig.Blasts().All(), [](const Explosion& explosion) {
        return explosion.phase == ExplosionPhase::Exploding;
    });
}

void HitRacerAhead(Rig& rig) {
    rig.Place(1, kAhead);
    rig.GiveWhite(1);
    rig.Fire(BrickColour::Red, 0);
    REQUIRE(rig.TicksUntil([&] { return rig.Car(1).Status().hitReactions > 0; }) > 0);
}

void ShieldRacerAhead(Rig& rig, std::uint8_t level) {
    rig.Place(1, kAhead);
    rig.Fire(BrickColour::Blue, level, 1);
    rig.Fire(BrickColour::Red, 0);
}

TEST_CASE("a cannonball fires one shot") {
    Rig rig;
    rig.Fire(BrickColour::Red, 0);

    REQUIRE(rig.Weapons().size() == 1);
    CHECK(rig.Weapons()[0].kind == WeaponEventKind::Fired);
}

TEST_CASE("a cannonball with nothing ahead lands on the track and leaves a scar") {
    Rig rig;
    rig.Fire(BrickColour::Red, 0);

    CHECK(rig.TicksUntil([&] { return rig.System().Active().empty(); }) == 241);
    CHECK(HasWeaponEvent(rig, WeaponEventKind::Landed));
    const Explosion& explosion = rig.Blasts().All()[0];
    CHECK(explosion.leavesScar);
    CHECK(explosion.position.x == doctest::Approx(5.0F + 520.1F).epsilon(0.001));
}

TEST_CASE("a cannonball that hits the racer ahead blows it up") {
    Rig rig(2);
    HitRacerAhead(rig);

    CHECK(HasWeaponEvent(rig, WeaponEventKind::Struck));
    CHECK(rig.System().Active().empty());
    CHECK(IsExploding(rig));
    CHECK(rig.Car(1).Body().motion.velocity.z == doctest::Approx(200.0F / kChassis.baseMass));
    CHECK(rig.Car(0).Status().hitReactions == 0);
}

TEST_CASE("a cannonball hit cheers its owner and dismays its victim") {
    Rig rig(2);
    HitRacerAhead(rig);

    CHECK(rig.Car(0).Events().reaction == VoiceReaction::Positive);
    CHECK(rig.Car(1).Events().reaction == VoiceReaction::Negative);
}

TEST_CASE("a racer hit by a cannonball drops a white brick") {
    Rig rig(2);
    HitRacerAhead(rig);

    CHECK(rig.Inventory(1).WhiteCount() == 0);
    CHECK(rig.Bricks().White()[0].brick.state == BrickState::Active);
}

TEST_CASE("a level 0 or 1 shield absorbs a cannonball, which still explodes") {
    Rig rig(2);
    ShieldRacerAhead(rig, 1);

    REQUIRE(rig.TicksUntil([&] { return IsExploding(rig); }) > 0);

    CHECK(rig.Car(1).Status().shield.absorbs == 1);
    CHECK(rig.Car(1).Status().hitReactions == 0);
    CHECK(rig.Car(1).Body().motion.velocity.z == doctest::Approx(0.0F));
}

TEST_CASE("a shield that absorbs a cannonball cheers only its racer") {
    Rig rig(2);
    ShieldRacerAhead(rig, 1);

    REQUIRE(rig.TicksUntil([&] { return IsExploding(rig); }) > 0);

    CHECK(rig.Car(1).Events().reaction == VoiceReaction::Positive);
    CHECK_FALSE(rig.Car(0).Events().reaction);
}

TEST_CASE("a level 2 shield turns a cannonball back without exploding") {
    Rig rig(2);
    ShieldRacerAhead(rig, 2);

    REQUIRE(rig.TicksUntil([&] { return rig.Car(1).Status().shield.absorbs > 0; }) > 0);

    CHECK_FALSE(IsExploding(rig));
    CHECK(rig.System().Active().size() == 2);
}

TEST_CASE("a cannonball turned back hits its owner, for the shielded racer") {
    Rig rig(2);
    ShieldRacerAhead(rig, 2);

    REQUIRE(rig.TicksUntil([&] { return rig.Car(0).Status().hitReactions > 0; }) > 0);

    CHECK(rig.Car(1).Status().hitReactions == 0);
    CHECK(rig.Blasts().All()[0].owner == 1);
}

TEST_CASE("a cannonball aims at the nearest racer ahead") {
    Rig rig(3);
    rig.Place(1, {.x = 305.0F, .z = 0.2F});
    rig.Place(2, kAhead);
    rig.Fire(BrickColour::Red, 0);

    REQUIRE(rig.TicksUntil([&] { return HasWeaponEvent(rig, WeaponEventKind::Struck); }) > 0);
    CHECK(rig.Car(2).Status().hitReactions == 1);
    CHECK(rig.Car(1).Status().hitReactions == 0);
}

TEST_CASE("a player's cannonball aims at a target point before a racer ahead") {
    Rig rig(2);
    rig.Place(1, kAhead);
    rig.AimAt({{.position = {.x = 205.0F, .y = 20.0F}}});
    rig.Fire(BrickColour::Red, 0);

    REQUIRE(rig.TicksUntil([&] { return HasWeaponEvent(rig, WeaponEventKind::Landed); }) > 0);
    const Vector3 landed = rig.Weapons()[0].position;
    CHECK(landed.x == doctest::Approx(205.0F).epsilon(0.02));
    CHECK(landed.y == doctest::Approx(20.0F).epsilon(0.05));
    CHECK(rig.Car(1).Status().hitReactions == 0);
}

}
TEST_CASE("a hazard's cannonball drops from its aim and can hit any racer") {
    Rig rig;
    rig.FireFromHazard(BrickColour::Red, 0,
                       {.source = {.x = 5.0F, .z = 30.0F}, .target = {.x = 5.0F, .z = 29.0F}});

    REQUIRE(rig.Weapons().size() == 1);
    CHECK_FALSE(rig.Weapons()[0].racer.has_value());
    CHECK(rig.Weapons()[0].position == Vector3{.x = 5.0F, .z = 30.0F});
    CHECK(rig.TicksUntil([&] { return rig.HasWeaponEvent(WeaponEventKind::Struck); }) > 0);
}

TEST_CASE("a hazard's cannonball flies at 30 with three times the gravity") {
    Rig rig;
    rig.FireFromHazard(
        BrickColour::Red, 0,
        {.source = {.y = 500.0F, .z = 100.0F}, .target = {.x = 30.0F, .y = 500.0F, .z = 100.0F}});

    rig.TickFor(84);

    CHECK(rig.EffectAt().x == doctest::Approx(30.24F).epsilon(0.01));
    CHECK(rig.EffectAt().z == doctest::Approx(99.611F).epsilon(0.001));
}

}
