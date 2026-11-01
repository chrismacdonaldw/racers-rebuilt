#include "sim/powerup/dynamite_effect.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>

#include "powerup_rig.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;
using test::Rig;

std::size_t LiveSpikes(const Rig& rig) {
    return static_cast<std::size_t>(std::ranges::count_if(
        rig.Spikes().All(),
        [](const Explosion& explosion) { return explosion.phase == ExplosionPhase::Exploding; }));
}

int TicksUntilLanded(Rig& rig) {
    return rig.TicksUntil([&] { return rig.HasWeaponEvent(WeaponEventKind::Landed); });
}

TEST_CASE("dynamite with no one behind lands behind its owner and blasts there") {
    Rig rig;
    rig.Fire(BrickColour::Yellow, 1);

    REQUIRE(TicksUntilLanded(rig) > 0);

    CHECK(rig.EffectAt().x < -50.0F);
    CHECK(LiveSpikes(rig) == 1);
    CHECK(rig.Spikes().All()[0].leavesScar);
}

TEST_CASE("dynamite blasts twice more, 500 ms apart") {
    Rig rig;
    rig.Fire(BrickColour::Yellow, 1);
    REQUIRE(TicksUntilLanded(rig) > 0);

    CHECK(rig.TicksUntil([&] { return LiveSpikes(rig) == 2; }) == 41);
    CHECK(rig.TicksUntil([&] { return rig.System().Active().empty(); }) == 42);
}

TEST_CASE("dynamite is thrown at a racer behind its owner") {
    Rig rig(2);
    rig.Place(1, {.x = -60.0F, .y = 10.0F, .z = 0.2F});
    rig.Fire(BrickColour::Yellow, 1);

    REQUIRE(TicksUntilLanded(rig) > 0);

    const Vector3 offset = rig.EffectAt() - rig.Car(1).Body().motion.position;
    CHECK(Length(offset) < 10.0F);
}

TEST_CASE("spike explosions launch racers near the dynamite") {
    Rig rig(2);
    rig.Place(1, {.x = -60.0F, .z = 0.2F});
    rig.Fire(BrickColour::Yellow, 1);

    REQUIRE(TicksUntilLanded(rig) > 0);

    CHECK(rig.TicksUntil([&] { return rig.Car(1).Body().motion.velocity.z > 0.0F; }, {}, 40) > 0);
}

}
}
