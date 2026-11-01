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

}
}
