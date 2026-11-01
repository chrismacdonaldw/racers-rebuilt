#include "sim/ai/ai_taunts.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr std::uint64_t kSeed = 7;

std::vector<Racer> Pair(const Vector3& other) {
    std::vector<Racer> racers;
    racers.emplace_back(ChassisSpec{}, CarBuild{}, RacerStart{.position = {.z = 0.2F}});
    racers.emplace_back(ChassisSpec{}, CarBuild{}, RacerStart{.position = other});
    return racers;
}

TEST_CASE("a computer racer taunts the racer just ahead, then waits 2 s plus a random draw") {
    const std::vector<Racer> racers = Pair({.x = 10.0F, .z = 0.2F});
    Random random(kSeed);
    Random twin(kSeed);
    AiTaunter taunter;

    CHECK(taunter.Tick(kTick, racers, 0, random));

    CHECK(taunter.Wait() == kTauntRetry + Milliseconds{8 * static_cast<int>(twin.Below(1024))});
    CHECK(random.Next() == twin.Next());
}

TEST_CASE("with nobody close ahead, a computer racer looks again after 2 s without a draw") {
    const std::vector<Racer> racers = Pair({.x = 14.0F, .z = 0.2F});
    Random random(kSeed);
    Random twin(kSeed);
    AiTaunter taunter;

    CHECK_FALSE(taunter.Tick(kTick, racers, 0, random));
    CHECK(taunter.Wait() == kTauntRetry);
    CHECK(random.Next() == twin.Next());
}

TEST_CASE("a racer beside or behind is no one to taunt") {
    Random random(kSeed);
    AiTaunter beside;
    AiTaunter behind;

    CHECK_FALSE(beside.Tick(kTick, Pair({.y = 10.0F, .z = 0.2F}), 0, random));
    CHECK_FALSE(behind.Tick(kTick, Pair({.x = -10.0F, .z = 0.2F}), 0, random));
}

TEST_CASE("a computer racer only looks once its wait runs out") {
    const std::vector<Racer> racers = Pair({.x = 14.0F, .z = 0.2F});
    std::vector<Racer> close = Pair({.x = 10.0F, .z = 0.2F});
    Random random(kSeed);
    AiTaunter taunter;
    CHECK_FALSE(taunter.Tick(kTick, racers, 0, random));

    bool hasTaunted = false;
    Milliseconds time{0};
    while (!hasTaunted) {
        hasTaunted = taunter.Tick(kTick, close, 0, random);
        time += kTick;
    }

    CHECK(time == Milliseconds{2004});
}

}
}
