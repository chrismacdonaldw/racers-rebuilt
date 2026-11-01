#include "sim/powerup/lightning_effect.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>

#include "powerup_rig.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;
using test::Rig;

constexpr Vector3 kAhead{.x = 105.0F, .z = 0.2F};
constexpr int kRampTicks = 42;
constexpr int kLifetimeTicks = 668;

std::size_t CountWeaponEvents(const Rig& rig, WeaponEventKind kind) {
    return static_cast<std::size_t>(std::ranges::count(rig.Weapons(), kind, &WeaponEvent::kind));
}

int TicksUntilStruck(Rig& rig, int limit = 1000) {
    return rig.TicksUntil([&] { return CountWeaponEvents(rig, WeaponEventKind::Struck) > 0; }, {},
                          limit);
}

TEST_CASE("a lightning bolt grows to 200 ahead of the car over half a second") {
    Rig rig;
    rig.Fire(BrickColour::Red, 2);
    rig.TicksUntil([] { return false; }, {}, kRampTicks / 2);

    CHECK(rig.EffectAt().x == doctest::Approx(5.0F + 100.0F).epsilon(0.05));

    rig.TicksUntil([] { return false; }, {}, kRampTicks);

    CHECK(rig.EffectAt().x == doctest::Approx(5.0F + 200.0F).epsilon(0.01));
}

TEST_CASE("lightning strikes the racer ahead once the bolt reaches it") {
    Rig rig(2);
    rig.Place(1, kAhead);
    rig.GiveWhite(1);
    rig.Fire(BrickColour::Red, 2);

    CHECK(TicksUntilStruck(rig) == kRampTicks);
    CHECK(rig.Car(1).Status().hitReactions == 1);
    CHECK(rig.Inventory(1).WhiteCount() == 0);
    CHECK(rig.Car(1).Body().motion.velocity.z > 0.0F);
    CHECK(rig.Car(0).Status().hitReactions == 0);
}

TEST_CASE("lightning holds its victim for a second before it can strike again") {
    Rig rig(2);
    rig.Place(1, kAhead);
    rig.Fire(BrickColour::Red, 2);
    CHECK_FALSE(rig.Effect().Progress().isHoldingRacer);
    REQUIRE(TicksUntilStruck(rig) > 0);
    CHECK(rig.Effect().Progress().isHoldingRacer);
    rig.Place(1, kAhead);

    CHECK(TicksUntilStruck(rig, 200) == 84);
}

TEST_CASE("a shield absorbs lightning without being held") {
    Rig rig(2);
    rig.Place(1, kAhead);
    rig.Fire(BrickColour::Blue, 0, 1);
    rig.Fire(BrickColour::Red, 2);

    REQUIRE(rig.TicksUntil([&] { return rig.Car(1).Status().shield.absorbs > 0; }) > 0);

    CHECK(rig.Car(1).Status().hitReactions == 0);
    CHECK(rig.Car(1).Body().motion.velocity.z < 0.01F);
    CHECK_FALSE(rig.Effect(1).Progress().isHoldingRacer);
}

TEST_CASE("lightning can't reach a racer past the end of the bolt") {
    Rig rig(2);
    rig.Place(1, {.x = 230.0F, .z = 0.2F});
    rig.Fire(BrickColour::Red, 2);

    CHECK(TicksUntilStruck(rig, kLifetimeTicks) == -1);
}

TEST_CASE("lightning lasts 8 seconds and ends with a thunderclap") {
    Rig rig;
    rig.Fire(BrickColour::Red, 2);

    CHECK(rig.TicksUntil([&] { return rig.System().Active().empty(); }) == kLifetimeTicks);
    CHECK(CountWeaponEvents(rig, WeaponEventKind::Ended) == 1);
}

}
}
