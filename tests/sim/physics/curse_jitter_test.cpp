#include "sim/physics/curse_jitter.hpp"

#include <doctest/doctest.h>

#include "sim/race/random.hpp"

namespace rr::sim {
namespace {

constexpr int kTickMs = 12;

TEST_CASE("curse jitter rolls a countdown, steer and throttle offset whenever it runs out") {
    CurseJitter jitter;
    Random random(3);

    UpdateCurseJitter(jitter, kTickMs, false, random);
    CHECK(jitter.countdownMs == 0);
    UpdateCurseJitter(jitter, kTickMs, true, random);

    CHECK(jitter.countdownMs < 500);
    CHECK(jitter.steerOffset >= -2.0F);
    CHECK(jitter.steerOffset < 2.0F);
    CHECK(jitter.throttleOffset >= -1.0F);
    CHECK(jitter.throttleOffset < 1.0F);
    const DriveCommand cursed =
        Cursed({.steering = 0.5F, .drive = 1.0F, .isDrifting = false, .isHardDrift = false},
               {.countdownMs = 0, .throttleOffset = 0.5F, .steerOffset = 0.0F});
    CHECK(cursed.steering == -0.5F);
    CHECK(cursed.drive == 1.0F);
}

}
}
