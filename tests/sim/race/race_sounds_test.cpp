#include "sim/race/race_sounds.hpp"

#include <doctest/doctest.h>

#include <vector>

namespace rr::sim {
namespace {

constexpr ChassisSpec kNeutralChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
constexpr Milliseconds kTick{12};

class Listener {
public:
    std::vector<SoundEvent> Hear() {
        sounds_.BeginTick();
        sounds_.HearRacer(0, racer_, true, true, kTick);
        return {sounds_.Events().begin(), sounds_.Events().end()};
    }

    Racer& Car() { return racer_; }
    Random& Rng() { return random_; }

private:
    RaceSounds sounds_{1, 0};
    Racer racer_{kNeutralChassis, {}, {.position = {.z = 0.2F}}};
    Random random_{5};
};

TEST_CASE("an absorbed hit plays the sound it rolled") {
    Listener listener;
    listener.Car().RaiseShield(1);
    CHECK(listener.Hear().empty());

    listener.Car().AbsorbShieldHit(listener.Rng());

    const std::vector<SoundEvent> sounds = listener.Hear();
    REQUIRE(sounds.size() == 1);
    CHECK(sounds[0].sound == RaceSound::ShieldAbsorb);
    CHECK(sounds[0].variant == listener.Car().Status().shield.absorbSound);
    CHECK(listener.Hear().empty());
}

TEST_CASE("a shield that ends plays its expiry, but one replaced by another doesn't") {
    Listener listener;
    listener.Car().RaiseShield(0);
    static_cast<void>(listener.Hear());

    listener.Car().RaiseShield(2);
    CHECK(listener.Hear().empty());

    listener.Car().LowerShield();
    const std::vector<SoundEvent> sounds = listener.Hear();
    REQUIRE(sounds.size() == 1);
    CHECK(sounds[0].sound == RaceSound::ShieldExpire);
}

TEST_CASE("weapon events play their fire and hit sounds where they happened") {
    RaceSounds sounds{3, 0, 6};
    sounds.BeginTick();
    const std::vector<WeaponEvent> events{
        {.kind = WeaponEventKind::Fired,
         .weapon = PowerupKind::Cannonball,
         .position = {.x = 1.0F}},
        {.kind = WeaponEventKind::Struck,
         .weapon = PowerupKind::Cannonball,
         .racer = 1,
         .position = {.x = 2.0F}},
        {.kind = WeaponEventKind::Fired, .weapon = PowerupKind::Missile, .position = {.x = 4.0F}},
        {.kind = WeaponEventKind::Struck,
         .weapon = PowerupKind::Missile,
         .racer = 2,
         .position = {.x = 5.0F}},
        {.kind = WeaponEventKind::Landed,
         .weapon = PowerupKind::Cannonball,
         .position = {.x = 3.0F}},
        {.kind = WeaponEventKind::Fired, .weapon = PowerupKind::GrapplingHook},
    };

    sounds.HearWeapons(events);

    CHECK(std::vector(sounds.Events().begin(), sounds.Events().end()) ==
          std::vector<SoundEvent>{
              {.sound = RaceSound::CannonFire, .position = {.x = 1.0F}},
              {.sound = RaceSound::CannonHit, .racer = 1, .position = {.x = 2.0F}},
              {.sound = RaceSound::MissileFire, .position = {.x = 4.0F}},
              {.sound = RaceSound::MissileHit, .racer = 2, .position = {.x = 5.0F}},
              {.sound = RaceSound::CannonHit, .position = {.x = 3.0F}},
          });
}

TEST_CASE("a scrape plays the sound it rolled") {
    Listener listener;
    listener.Car().Scrape(1);

    const std::vector<SoundEvent> sounds = listener.Hear();

    REQUIRE(sounds.size() == 1);
    CHECK(sounds[0].sound == RaceSound::CarScrape);
    CHECK(sounds[0].variant == 1);
}

}
}
