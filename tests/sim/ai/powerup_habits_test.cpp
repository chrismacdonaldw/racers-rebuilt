#include "sim/ai/powerup_habits.hpp"

#include <doctest/doctest.h>

#include <cstddef>

namespace rr::sim {
namespace {

using assets::BrickColour;

TEST_CASE("a driver stat becomes a use chance from 51 at 0 to 255 at 90 and above") {
    CHECK(UseChance(0) == 51);
    CHECK(UseChance(45) == 153);
    CHECK(UseChance(75) == 221);
    CHECK(UseChance(90) == 255);
    CHECK(UseChance(100) == 255);
}

TEST_CASE("a driver's habits give each colour its own stat's chance") {
    const PowerupHabits habits =
        HabitsOf({.redStat = 0, .yellowStat = 45, .greenStat = 75, .blueStat = 90});

    CHECK(habits.UseChance(BrickColour::Red) == 51);
    CHECK(habits.UseChance(BrickColour::Yellow) == 153);
    CHECK(habits.UseChance(BrickColour::Green) == 221);
    CHECK(habits.UseChance(BrickColour::Blue) == 255);
}

TEST_CASE("a mod driver's use chances and start boost override its stats") {
    assets::DriverEntry driver{.redStat = 0, .greenStat = 75};
    driver.useChances[static_cast<std::size_t>(BrickColour::Red)] = 200;
    driver.startBoost = true;

    const PowerupHabits habits = HabitsOf(driver);

    CHECK(habits.UseChance(BrickColour::Red) == 200);
    CHECK(habits.UseChance(BrickColour::Green) == 221);
    CHECK(habits.startBoost == true);
}

TEST_CASE("a driver's charge colour counts red, blue, green, yellow from 1") {
    CHECK(HabitsOf({.aiChargeColor = 1}).chargeColour == BrickColour::Red);
    CHECK(HabitsOf({.aiChargeColor = 2}).chargeColour == BrickColour::Blue);
    CHECK(HabitsOf({.aiChargeColor = 3, .aiWhiteBrickTarget = 3}).chargeColour ==
          BrickColour::Green);
    CHECK(HabitsOf({.aiChargeColor = 3, .aiWhiteBrickTarget = 3}).chargeWhites == 3);
    CHECK(HabitsOf({.aiChargeColor = 4}).chargeColour == BrickColour::Yellow);
}

TEST_CASE("a driver with no charge colour charges nothing") {
    CHECK_FALSE(HabitsOf({.aiChargeColor = 0, .aiWhiteBrickTarget = 2}).chargeColour);
    CHECK(HabitsOf({.aiChargeColor = 5, .aiWhiteBrickTarget = 2}).chargeWhites == 0);
}

}
}
