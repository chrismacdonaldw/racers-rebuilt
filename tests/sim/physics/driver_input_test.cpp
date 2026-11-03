#include "sim/physics/driver_input.hpp"

#include <doctest/doctest.h>

namespace rr::sim {
namespace {

constexpr int kTickMs = 12;

DriveCommand Hold(DriverInput& driver, const RacerControls& controls, int durationMs,
                  bool canStartDrift = false,
                  assets::GameVersion version = assets::GameVersion::Release1999) {
    DriveCommand command;
    for (int elapsed = 0; elapsed < durationMs; elapsed += kTickMs) {
        command = driver.Update(controls, canStartDrift, RulesFor(version), kTickMs);
    }
    return command;
}

float Hold2001(DriverInput& driver, const RacerControls& controls, int durationMs) {
    return Hold(driver, controls, durationMs, false, assets::GameVersion::Release2001).steering;
}

TEST_CASE("a held steer key ramps toward full lock at 1.25 per second") {
    DriverInput driver;

    CHECK(Hold(driver, {.isLeftHeld = true}, 480).steering == doctest::Approx(0.6F));
    CHECK(Hold(driver, {.isLeftHeld = true}, 1200).steering == 1.0F);
    CHECK(Hold(driver, {.isRightHeld = true}, 120).steering == doctest::Approx(0.01F));
}

TEST_CASE("released steering returns to centre without crossing it") {
    DriverInput driver;
    static_cast<void>(Hold(driver, {.isLeftHeld = true}, 480));

    CHECK(Hold(driver, {}, 36).steering == doctest::Approx(0.6F - (8.25F * 0.036F)));
    CHECK(Hold(driver, {}, 120).steering == 0.0F);
}

TEST_CASE("both steer keys return to centre") {
    DriverInput driver;
    static_cast<void>(Hold(driver, {.isRightHeld = true}, 480));

    CHECK(Hold(driver, {.isLeftHeld = true, .isRightHeld = true}, 120).steering == 0.0F);
}

TEST_CASE("a part-deflected stick steers faster up to its own limit") {
    DriverInput driver;

    const float early = Hold(driver, {.steerLeftAxis = 0.5F}, 96).steering;

    CHECK(early == doctest::Approx(2.5F * 0.75F * 0.096F));
    CHECK(Hold(driver, {.steerLeftAxis = 0.5F}, 600).steering == 0.5F);
    CHECK(Hold(driver, {.steerLeftAxis = 0.25F}, 12).steering == 0.25F);
}

TEST_CASE("2001 recentres released keys three times faster") {
    DriverInput driver;
    static_cast<void>(Hold2001(driver, {.isLeftHeld = true}, 480));

    CHECK(Hold2001(driver, {}, 12) == doctest::Approx(0.6F - (25.25F * 0.012F)));
}

TEST_CASE("2001 steers a part-deflected stick on a squared curve") {
    DriverInput driver;

    CHECK(Hold2001(driver, {.steerLeftAxis = 0.5F}, 96) ==
          doctest::Approx(4.15F * (0.45F + (0.5F * 0.75F)) * 0.096F));
}

TEST_CASE("reversing the stick recentres before turning in 2001, and in 1999 eases back") {
    DriverInput original;
    DriverInput rerelease;
    static_cast<void>(Hold(original, {.steerLeftAxis = 1.0F}, 1200));
    static_cast<void>(Hold2001(rerelease, {.steerLeftAxis = 1.0F}, 1200));

    CHECK(Hold(original, {.steerLeftAxis = -1.0F}, 12).steering ==
          doctest::Approx(1.0F - (8.25F * 0.012F)));
    CHECK(Hold2001(rerelease, {.steerLeftAxis = -1.0F}, 12) ==
          doctest::Approx(-4.15F * 0.45F * 0.012F));
}

TEST_CASE("throttle and brake together give half drive") {
    DriverInput driver;

    CHECK(Hold(driver, {.isThrottleHeld = true}, 12).drive == 1.0F);
    CHECK(Hold(driver, {.isBrakeHeld = true}, 12).drive == -1.0F);
    CHECK(Hold(driver, {.isThrottleHeld = true, .isBrakeHeld = true}, 12).drive == 0.5F);
}

TEST_CASE("a drift needs throttle, steering and a body that allows it") {
    DriverInput driver;
    const RacerControls drifting{.isLeftHeld = true, .isThrottleHeld = true, .isDriftHeld = true};

    CHECK_FALSE(Hold(driver, drifting, 120, false).isDrifting);
    const DriveCommand started = Hold(driver, drifting, 12, true);
    CHECK(started.isDrifting);
    CHECK_FALSE(started.isHardDrift);
    CHECK(started.drive == 1.0F);
    CHECK_FALSE(Hold(driver, {.isLeftHeld = true, .isDriftHeld = true}, 12, true).isDrifting);
}

TEST_CASE("braking while starting a drift makes it a hard drift") {
    DriverInput driver;
    const RacerControls hard{
        .isLeftHeld = true, .isThrottleHeld = true, .isBrakeHeld = true, .isDriftHeld = true};

    const DriveCommand command = Hold(driver, hard, 24, true);

    CHECK(command.isHardDrift);
    CHECK(command.drive == 1.0F);
}

}
}
