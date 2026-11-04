#include "sim/ai/ai_powerups.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

namespace rr::sim {
namespace {

using assets::BrickColour;

constexpr Milliseconds kTick{12};
constexpr ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
constexpr int kFirstCheck = 13;
constexpr int kHeldCheck = kFirstCheck + 84;
constexpr int kManyTicks = 200;
constexpr std::uint64_t kSeed = 7;

std::uint8_t Roll(Random& random) {
    return static_cast<std::uint8_t>(random.Below(256));
}

PowerupHabits Chance(BrickColour colour, std::uint8_t chance) {
    PowerupHabits habits;
    habits.useChances[static_cast<std::size_t>(colour)] = chance;
    return habits;
}

struct Scene {
    explicit Scene(std::uint64_t seed = kSeed) : random(seed), twin(seed) {}

    [[nodiscard]] AiPowerupView View() const {
        return {.racers = racers,
                .inventory = &inventory,
                .isCrowded = isCrowded,
                .hasFinished = hasFinished,
                .isLiningUp = isLiningUp};
    }

    int TicksToUse(AiPowerupUser& user, int limit = kManyTicks) {
        for (int tick = 1; tick <= limit; ++tick) {
            if (user.Tick(kTick, View(), random)) {
                return tick;
            }
        }
        return -1;
    }

    [[nodiscard]] bool HasRolled() { return random.Next() != twin.Next(); }

    std::vector<Racer> racers{Racer(kChassis, {}, {.position = {.z = 0.2F}})};
    PowerupInventory inventory;
    bool isCrowded = false;
    bool hasFinished = false;
    bool isLiningUp = false;
    Random random;
    Random twin;
};

void AddRacerAt(Scene& scene, float x) {
    scene.racers.emplace_back(kChassis, CarBuild{}, RacerStart{.position = {.x = x, .z = 0.2F}});
}

std::uint64_t SeedWithFallingRolls() {
    for (std::uint64_t seed = 1;; ++seed) {
        Random random(seed);
        const std::uint8_t first = Roll(random);
        if (Roll(random) < first) {
            return seed;
        }
    }
}

TEST_CASE("a computer racer first checks its power-up 150 ms in, and uses it on a good roll") {
    Scene scene;
    scene.inventory.Grant(BrickColour::Yellow, 0);
    const std::uint8_t roll = Roll(scene.twin);
    REQUIRE(roll < kSureUseChance);
    AiPowerupUser user(Chance(BrickColour::Yellow, roll + 1));

    CHECK(scene.TicksToUse(user) == kFirstCheck);
}

TEST_CASE("a computer racer that rolls badly waits 1000 ms before it checks again") {
    Scene scene(SeedWithFallingRolls());
    scene.inventory.Grant(BrickColour::Blue, 0);
    const std::uint8_t first = Roll(scene.twin);
    AiPowerupUser user(Chance(BrickColour::Blue, first));

    CHECK(scene.TicksToUse(user) == kHeldCheck);
}

TEST_CASE("a computer racer holds red until a racer is in the cone ahead, without rolling") {
    Scene scene;
    scene.inventory.Grant(BrickColour::Red, 0);
    AddRacerAt(scene, 300.0F);
    AiPowerupUser user(Chance(BrickColour::Red, Roll(scene.twin) + 1));

    CHECK(scene.TicksToUse(user, kFirstCheck * 2) == -1);
    scene.twin = Random(kSeed);
    CHECK_FALSE(scene.HasRolled());

    scene.random = Random(kSeed);
    scene.racers[1].Place({.position = {.x = 50.0F, .z = 0.2F}});
    CHECK(scene.TicksToUse(user) == kFirstCheck);
}

TEST_CASE("a lining-up computer racer holds yellow until a racer is behind it, without rolling") {
    Scene scene;
    scene.isLiningUp = true;
    scene.inventory.Grant(BrickColour::Yellow, 0);
    AddRacerAt(scene, -150.0F);
    AiPowerupUser user(Chance(BrickColour::Yellow, Roll(scene.twin) + 1));

    CHECK(scene.TicksToUse(user, kFirstCheck * 2) == -1);
    scene.twin = Random(kSeed);
    CHECK_FALSE(scene.HasRolled());

    scene.random = Random(kSeed);
    scene.racers[1].Place({.position = {.x = -50.0F, .z = 0.2F}});
    CHECK(scene.TicksToUse(user) == kFirstCheck);
}

TEST_CASE("a computer racer that isn't lining up drops yellow on its roll alone") {
    Scene scene;
    scene.inventory.Grant(BrickColour::Yellow, 0);
    AddRacerAt(scene, -150.0F);
    AiPowerupUser user(Chance(BrickColour::Yellow, Roll(scene.twin) + 1));

    CHECK(scene.TicksToUse(user) == kFirstCheck);
}

TEST_CASE("a computer racer holds everything while the effects are crowded") {
    Scene scene;
    scene.inventory.Grant(BrickColour::Green, 0);
    scene.isCrowded = true;
    AiPowerupUser user(Chance(BrickColour::Green, kSureUseChance));

    CHECK(scene.TicksToUse(user, kFirstCheck) == -1);
    CHECK_FALSE(scene.HasRolled());
}

TEST_CASE("a crowded check waits 1000 ms before the next") {
    Scene scene;
    scene.inventory.Grant(BrickColour::Yellow, 0);
    scene.isCrowded = true;
    AiPowerupUser user(Chance(BrickColour::Yellow, Roll(scene.twin) + 1));
    REQUIRE(scene.TicksToUse(user, kFirstCheck) == -1);

    scene.isCrowded = false;

    CHECK(scene.TicksToUse(user) == kHeldCheck - kFirstCheck);
}

TEST_CASE("a computer racer charges its charge colour up to its white-brick target") {
    Scene scene;
    PowerupHabits habits = Chance(BrickColour::Green, kSureUseChance);
    habits.chargeColour = BrickColour::Green;
    habits.chargeWhites = 2;
    AiPowerupUser user(habits);
    scene.inventory.Grant(BrickColour::Green, 1);

    CHECK(scene.TicksToUse(user, kFirstCheck) == -1);
    CHECK_FALSE(scene.HasRolled());
}

TEST_CASE("a computer racer uses its charge colour once charged") {
    Scene scene;
    PowerupHabits habits = Chance(BrickColour::Green, Roll(scene.twin) + 1);
    habits.chargeColour = BrickColour::Green;
    habits.chargeWhites = 2;
    AiPowerupUser user(habits);
    scene.inventory.Grant(BrickColour::Green, 2);

    CHECK(scene.TicksToUse(user) == kFirstCheck);
}

TEST_CASE("a spinning computer racer rolls for green but rechecks after 150 ms") {
    Scene scene;
    scene.inventory.Grant(BrickColour::Green, 0);
    scene.racers[0].StartSpin(4.0F, 0.001F, 1.0F);
    AiPowerupUser user(Chance(BrickColour::Green, kSureUseChance));

    CHECK(scene.TicksToUse(user, kFirstCheck * 2) == -1);
    static_cast<void>(scene.twin.Next());
    static_cast<void>(scene.twin.Next());
    CHECK_FALSE(scene.HasRolled());
}

TEST_CASE("a finished computer racer keeps a fully charged green") {
    Scene scene;
    scene.hasFinished = true;
    scene.inventory.Grant(BrickColour::Green, kMaxWhiteBricks);
    AiPowerupUser user(Chance(BrickColour::Green, Roll(scene.twin) + 1));

    CHECK(scene.TicksToUse(user) == -1);
}

TEST_CASE("a finished computer racer still uses a green with fewer white bricks") {
    Scene scene;
    scene.hasFinished = true;
    scene.inventory.Grant(BrickColour::Green, 2);
    AiPowerupUser user(Chance(BrickColour::Green, Roll(scene.twin) + 1));

    CHECK(scene.TicksToUse(user) == kFirstCheck);
}

TEST_CASE("a computer racer holding nothing never rolls") {
    Scene scene;
    AiPowerupUser user(Chance(BrickColour::Red, kSureUseChance));

    CHECK(scene.TicksToUse(user) == -1);
    CHECK_FALSE(scene.HasRolled());
}

TEST_CASE("a start boost needs a roll below the green chance") {
    Random twin(kSeed);
    const std::uint8_t roll = Roll(twin);
    Random random(kSeed);

    CHECK_FALSE(AiPowerupUser(Chance(BrickColour::Green, roll)).RollsStartBoost(random));
    random = Random(kSeed);
    CHECK(AiPowerupUser(Chance(BrickColour::Green, roll + 1)).RollsStartBoost(random));
}

TEST_CASE("a set start boost skips the roll") {
    PowerupHabits always = Chance(BrickColour::Green, 0);
    always.startBoost = true;
    PowerupHabits never = Chance(BrickColour::Green, kSureUseChance);
    never.startBoost = false;
    Random random(kSeed);

    CHECK(AiPowerupUser(always).RollsStartBoost(random));
    CHECK_FALSE(AiPowerupUser(never).RollsStartBoost(random));
    CHECK(random.Next() == Random(kSeed).Next());
}

}
}
