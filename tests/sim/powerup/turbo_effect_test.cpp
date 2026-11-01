#include "sim/powerup/turbo_effect.hpp"

#include <doctest/doctest.h>

#include <cstdint>

#include "../ai/straight_route.hpp"
#include "powerup_rig.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;
using test::CountEnded;
using test::Rig;
using test::RouteEffectsOf;

void CheckTurboLevel(std::uint8_t level, int ticks) {
    CAPTURE(level);
    Rig rig;
    rig.Fire(BrickColour::Green, level);

    CHECK(rig.Car().Body().maxSpeed == 176.0F);
    CHECK(rig.TicksUntilIdle() == ticks);
    CHECK(rig.Car().Body().maxSpeed == 120.0F);
}

TEST_CASE("each turbo level lasts the racer's turbo, at the boosted top speed") {
    CheckTurboLevel(0, 177);
    CheckTurboLevel(1, 218);
    CheckTurboLevel(2, 510);
}

TEST_CASE("a turbo cheers its racer") {
    Rig rig;
    rig.Fire(BrickColour::Green, 0);

    CHECK(rig.Car().Events().reaction == VoiceReaction::Positive);
}

TEST_CASE("a level 2 turbo halts the racer until its effect ends") {
    Rig rig;
    rig.Fire(BrickColour::Green, 2);

    REQUIRE(rig.System().Active().size() == 1);
    CHECK(rig.System().Slots()[rig.System().Active()[0]].kind == PowerupKind::Turbo);
    CHECK(rig.Car().Status().isHalted);
    CHECK(rig.TicksUntilIdle() == 510);
    CHECK_FALSE(rig.Car().Status().isHalted);
}

TEST_CASE("a second turbo ends the first one's effect") {
    Rig rig;
    rig.Fire(BrickColour::Green, 2);
    rig.Tick();

    rig.Fire(BrickColour::Green, 1);
    rig.Tick();

    CHECK(CountEnded(rig.System()) == 1);
    REQUIRE(rig.System().Active().size() == 1);
    CHECK(rig.System().Slots()[rig.System().Active()[0]].use.level == 1);
    CHECK(rig.Car().ActiveTurbo().transform([](const Turbo& turbo) { return turbo.level; }) ==
          TurboLevel::Strong);
    CHECK_FALSE(rig.Car().Status().isHalted);
}

TEST_CASE("a turbo taken back by a full pool stops that racer's turbo") {
    Rig rig(7);
    for (std::uint8_t racer = 0; racer < 7; ++racer) {
        rig.Fire(BrickColour::Green, 0, racer);
    }

    CHECK(rig.System().Active().size() == 6);
    CHECK_FALSE(rig.Car(0).ActiveTurbo().has_value());
    CHECK(rig.Car(6).ActiveTurbo().has_value());
}

TEST_CASE("a computer racer's turbo plays its route at 1.7 until the turbo ends") {
    Rig rig;
    rig.Car().FollowRoute(test::StraightRoute({.z = 0.2F}));
    rig.Fire(BrickColour::Green, 1);
    rig.Tick();

    REQUIRE(rig.Car().Route().has_value());
    CHECK(rig.Car().Route().transform(&AiRacer::PlaybackSpeed) == 1.7F);
    CHECK(rig.TicksUntilIdle() == 217);
    CHECK_FALSE(RouteEffectsOf(rig.Car()).isBoosting);
    CHECK(rig.Car().Route().transform(&AiRacer::TargetSpeed) == 1.0F);
}

}
}
