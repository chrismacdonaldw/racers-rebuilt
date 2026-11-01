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

TEST_CASE("a racer's first voice reaction since its last tick plays") {
    Listener listener;
    listener.Car().React(VoiceReaction::Negative);
    listener.Car().React(VoiceReaction::Positive);

    const std::vector<SoundEvent> sounds = listener.Hear();

    REQUIRE(sounds.size() == 1);
    CHECK(sounds[0].sound == RaceSound::NegativeReaction);
}

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

TEST_CASE("yellow traps play their drop, touch, blast and fade sounds") {
    RaceSounds sounds{2, 0, 6};
    sounds.BeginTick();
    const std::vector<WeaponEvent> events{
        {.kind = WeaponEventKind::Fired, .weapon = PowerupKind::OilSlick},
        {.kind = WeaponEventKind::Struck, .weapon = PowerupKind::OilSlick, .racer = 1},
        {.kind = WeaponEventKind::Landed, .weapon = PowerupKind::Dynamite},
        {.kind = WeaponEventKind::Fired, .weapon = PowerupKind::Magnet},
        {.kind = WeaponEventKind::Struck, .weapon = PowerupKind::Magnet, .racer = 1},
        {.kind = WeaponEventKind::Ended, .weapon = PowerupKind::Magnet},
    };

    sounds.HearWeapons(events);

    CHECK(std::vector(sounds.Events().begin(), sounds.Events().end()) ==
          std::vector<SoundEvent>{
              {.sound = RaceSound::OilDrop},
              {.sound = RaceSound::OilSlip, .racer = 1},
              {.sound = RaceSound::DynamiteBlast},
              {.sound = RaceSound::MagnetDeploy},
              {.sound = RaceSound::MagnetGrab, .racer = 1},
              {.sound = RaceSound::MagnetFade},
          });
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

TEST_CASE("grappling hook and lightning events play their sounds") {
    RaceSounds sounds{2, 0, 6};
    sounds.BeginTick();
    const std::vector<WeaponEvent> events{
        {.kind = WeaponEventKind::Fired, .weapon = PowerupKind::GrapplingHook},
        {.kind = WeaponEventKind::Struck, .weapon = PowerupKind::GrapplingHook, .racer = 1},
        {.kind = WeaponEventKind::Ended, .weapon = PowerupKind::GrapplingHook},
        {.kind = WeaponEventKind::Landed, .weapon = PowerupKind::GrapplingHook},
        {.kind = WeaponEventKind::Struck, .weapon = PowerupKind::Lightning, .racer = 1},
        {.kind = WeaponEventKind::Ended, .weapon = PowerupKind::Lightning},
    };

    sounds.HearWeapons(events);

    std::vector<RaceSound> heard;
    for (const SoundEvent& event : sounds.Events()) {
        heard.push_back(event.sound);
    }
    CHECK(heard == std::vector{RaceSound::GrappleFire, RaceSound::GrappleHit,
                               RaceSound::GrappleRelease, RaceSound::GrappleMiss,
                               RaceSound::LightningStrike, RaceSound::LightningEnd});
}

}
}
