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

}
}
