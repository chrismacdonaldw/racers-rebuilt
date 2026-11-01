#include "sim/powerup/shield_effect.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "sim/powerup/powerup_system.hpp"

namespace rr::sim {
namespace {

using assets::BrickColour;

constexpr ChassisSpec kNeutralChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
constexpr Milliseconds kTick{10};
constexpr std::array kDurations{Milliseconds{4000}, Milliseconds{6000}, Milliseconds{8000},
                                Milliseconds{10'000}};
constexpr Milliseconds kFade{1000};

class Shields {
public:
    Shields()
        : system_(Factories(), racers_.size()), context_{.racers = racers_, .random = &random_} {}

    void Raise(std::uint8_t level, std::uint8_t racer = 0) {
        system_.BeginTick();
        system_.Fire({.racer = racer, .colour = BrickColour::Blue, .level = level}, context_);
    }

    void Run(Milliseconds time) {
        for (Milliseconds run{0}; run < time; run += kTick) {
            system_.BeginTick();
            system_.Tick(kTick, context_);
            for (Racer& racer : racers_) {
                racer.UpdateTimers(kTick);
            }
        }
    }

    bool Hit(std::uint8_t racer) {
        return system_.Hit({.racer = racer, .from = 1, .by = PowerupKind::Cannonball}, context_);
    }

    Racer& RacerAt(std::size_t index) { return racers_.at(index); }
    PowerupSystem& System() { return system_; }

private:
    static EffectFactories Factories() {
        EffectFactories factories;
        AddBlueEffects(factories);
        return factories;
    }

    std::vector<Racer> racers_{Racer(kNeutralChassis, {}, {.position = {.z = 0.2F}}),
                               Racer(kNeutralChassis, {}, {.position = {.y = 20.0F, .z = 0.2F}})};
    Random random_{7};
    PowerupSystem system_;
    PowerupContext context_;
};

void CheckLifetime(std::uint8_t level) {
    Shields shields;
    shields.Raise(level);
    CHECK(shields.RacerAt(0).Status().shield.level == level);

    shields.Run(kDurations.at(level) + kFade - kTick);
    CHECK(shields.RacerAt(0).IsShielded());

    shields.Run(kTick);
    CHECK_FALSE(shields.RacerAt(0).IsShielded());
    CHECK(shields.System().EntitiesInUse() == 0);
}

TEST_CASE("each shield level lasts its duration, then a 1,000 ms fade") {
    for (std::uint8_t level = 0; level <= kMaxPowerupLevel; ++level) {
        CAPTURE(level);
        CheckLifetime(level);
    }
}

TEST_CASE("a shield holds 2 entities and fades once its duration ends") {
    Shields shields;
    shields.Raise(1);
    CHECK(shields.System().EntitiesInUse() == 2);

    shields.Run(kDurations[1] - kTick);
    const PowerupEffect& effect = *shields.System().Slots()[shields.System().Active()[0]].effect;
    CHECK(effect.Progress().phase == 0);
    shields.Run(kTick);
    CHECK(effect.Progress() == EffectProgress{.phase = 1, .phaseTime = Milliseconds{0}});
}

TEST_CASE("a shield sits 2 above its racer") {
    Shields shields;
    shields.Raise(1, 1);

    shields.Run(kTick);

    const Transform pose = shields.System().Slots()[shields.System().Active()[0]].effect->Pose();
    CHECK(pose.position.y == shields.RacerAt(1).Pose().position.y);
    CHECK(pose.position.z == doctest::Approx(shields.RacerAt(1).Pose().position.z + 2.0F));
}

TEST_CASE("raising a shield lifts a curse") {
    Shields shields;
    shields.RacerAt(0).AttachCurse(Milliseconds{10'000});

    shields.Raise(0);

    CHECK_FALSE(shields.RacerAt(0).Status().isCursed);
}

TEST_CASE("a new shield replaces the racer's old one") {
    Shields shields;
    shields.Raise(3);
    shields.Run(Milliseconds{1000});

    shields.Raise(0);
    shields.Run(kTick);

    CHECK(shields.RacerAt(0).Status().shield.level == 0);
    CHECK(shields.System().Active().size() == 1);
    CHECK(shields.System().EntitiesInUse() == 2);
    shields.Run(kDurations[0] + kFade);
    CHECK_FALSE(shields.RacerAt(0).IsShielded());
}

TEST_CASE("a shield stops hits on its own racer only") {
    Shields shields;
    shields.Raise(0);

    CHECK(shields.Hit(0));
    CHECK_FALSE(shields.Hit(1));
}

TEST_CASE("an absorbed hit rolls its sound, then waits 500 ms before the next") {
    Shields shields;
    shields.Raise(2);

    REQUIRE(shields.Hit(0));
    const ShieldStatus first = shields.RacerAt(0).Status().shield;
    CHECK(first.absorbs == 1);
    CHECK(first.absorbSound < 3);
    CHECK(first.cooldownMs == 500);

    shields.Run(Milliseconds{490});
    CHECK(shields.Hit(0));
    CHECK(shields.RacerAt(0).Status().shield.absorbs == 1);

    shields.Run(kTick);
    CHECK(shields.Hit(0));
    CHECK(shields.RacerAt(0).Status().shield.absorbs == 2);
}

TEST_CASE("an ended shield stops no hits") {
    Shields shields;
    shields.Raise(0);

    shields.Run(kDurations[0] + kFade);

    CHECK_FALSE(shields.Hit(0));
}

TEST_CASE("shots turn back from shields at level 2 and up") {
    const std::array expected{false, false, true, true};
    for (std::uint8_t level = 0; level <= kMaxPowerupLevel; ++level) {
        CAPTURE(level);
        Racer racer(kNeutralChassis, {}, {});
        CHECK_FALSE(TurnsShotsBack(racer));

        racer.RaiseShield(level);

        CHECK(TurnsShotsBack(racer) == expected.at(level));
    }
}

}
}
