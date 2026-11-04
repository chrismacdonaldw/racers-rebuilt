#include "sim/powerup/powerup_tuning.hpp"

#include <doctest/doctest.h>

#include <optional>

#include "powerup_rig.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;
using test::Rig;

std::uint8_t PhaseOf(const Rig& rig, std::size_t active) {
    return rig.Effect(active).Progress().phase;
}

}

TEST_CASE("missiles launch at their tuned arm times") {
    Rig rig;
    PowerupTuning tuning;
    tuning.missile.armTimes = {Milliseconds{120}, Milliseconds{240}, Milliseconds{360}};
    rig.Tune(tuning);
    rig.Fire(BrickColour::Red, 3);

    rig.TickFor(10);

    CHECK(PhaseOf(rig, 0) == static_cast<std::uint8_t>(ShotPhase::Flying));
    CHECK(PhaseOf(rig, 1) == static_cast<std::uint8_t>(ShotPhase::Armed));
    CHECK(PhaseOf(rig, 2) == static_cast<std::uint8_t>(ShotPhase::Armed));
}

TEST_CASE("an untouched oil slick lasts its tuned lifetime") {
    Rig rig;
    PowerupTuning tuning;
    tuning.oilSlick.lifetime = Milliseconds{5000};
    rig.Tune(tuning);
    rig.Fire(BrickColour::Yellow, 0);

    CHECK(rig.TicksUntilIdle() == 418);
}

TEST_CASE("a shield holds for its tuned duration and fade") {
    Rig rig;
    PowerupTuning tuning;
    tuning.shield.durations[0] = Milliseconds{1000};
    tuning.shield.fade = Milliseconds{500};
    rig.Tune(tuning);
    rig.Fire(BrickColour::Blue, 0);

    CHECK(rig.TicksUntilIdle() == 125);
    CHECK_FALSE(rig.Car().IsShielded());
}

TEST_CASE("a green level gives its tuned turbo") {
    Rig rig;
    PowerupTuning tuning;
    tuning.turbo.levels[0] = TurboLevel::Hover;
    rig.Tune(tuning);

    rig.Fire(BrickColour::Green, 0);

    const std::optional<Turbo>& turbo = rig.Car().ActiveTurbo();
    CHECK(turbo.transform([](const Turbo& active) { return active.level; }) == TurboLevel::Hover);
}

}
