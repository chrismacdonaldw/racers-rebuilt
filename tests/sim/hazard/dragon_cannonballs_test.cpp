#include "sim/hazard/dragon_cannonballs.hpp"

#include <doctest/doctest.h>

#include <cmath>

#include "hazard_rig.hpp"

namespace rr::sim {
namespace {

constexpr assets::EventId kTrigger = 4;
constexpr Vector3 kDragon{.x = 100.0F, .z = 50.0F};

assets::Hazard Dragon() {
    return {.kind = assets::HazardKind::DragonCannonballs,
            .payload = assets::DragonCannonballs{
                .entity = "prntdum", .trigger = kTrigger, .interval = Milliseconds{1500}}};
}

bool IsOffset(float value, float most) {
    return value == std::round(value) && value >= -2.0F && value <= most;
}

bool IsDrop(const Vector3& drop) {
    return drop.z == 1.0F && (drop.x != 0.0F || drop.y != 0.0F) && IsOffset(drop.x, 2.0F) &&
           IsOffset(drop.y, 1.0F);
}

}

TEST_CASE("the dragon drops an ownerless ball on its first tick, near its emitter") {
    test::HazardRig rig({Dragon()}, test::StillEntities({"prntdum"}, kDragon));
    rig.Hear(kTrigger, EventEdge::Start);

    rig.Tick();

    REQUIRE(rig.Powerups().Active().size() == 1);
    CHECK(rig.Effect().kind == PowerupKind::Cannonball);
    CHECK(rig.Effect().use.hazard == 0);
    const HazardAim& aim = rig.Hazards().Aims()[0];
    CHECK(aim.source == Vector3{.x = 65.0F, .z = 50.0F});
    CHECK(IsDrop(aim.target - aim.source));
}

TEST_CASE("the dragon drops a ball every interval while it runs") {
    test::HazardRig rig({Dragon()}, test::StillEntities({"prntdum"}, kDragon));
    rig.Hear(kTrigger, EventEdge::Start);
    rig.Tick();

    for (int tick = 0; tick < 124; ++tick) {
        rig.Tick();
    }
    CHECK(rig.Powerups().Active().size() == 1);
    rig.Tick();

    CHECK(rig.Powerups().Active().size() == 2);
}

TEST_CASE("the dragon stops when its trigger ends") {
    test::HazardRig rig({Dragon()}, test::StillEntities({"prntdum"}, kDragon));
    rig.Hear(kTrigger, EventEdge::Start);
    rig.Hear(kTrigger, EventEdge::End);

    rig.Tick();

    CHECK(rig.Powerups().Active().empty());
}

}
