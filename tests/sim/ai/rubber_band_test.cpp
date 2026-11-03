#include "sim/ai/rubber_band.hpp"

#include <doctest/doctest.h>

#include <array>
#include <vector>

namespace rr::sim {
namespace {

constexpr Milliseconds kDelay{15000};
constexpr Milliseconds kTick{12};

std::vector<PacedRacer> Grid() {
    return {
        {.lapProgress = 1.0F, .isAiControlled = false},
        {.lapProgress = 1.5F, .isAiControlled = true},
        {.lapProgress = 0.5F, .isAiControlled = true},
        {.lapProgress = 1.0F, .isAiControlled = true},
    };
}

std::array<float, 4> Paced(float bonus, std::vector<PacedRacer> racers) {
    RubberBand band(bonus);
    std::array<float, 4> speeds{2.0F, 2.0F, 2.0F, 2.0F};
    band.Update(kDelay, racers, speeds);
    band.Update(kTick, racers, speeds);
    return speeds;
}

TEST_CASE("the rubber band waits 15 s before pacing") {
    RubberBand band(0.0F);
    const std::vector<PacedRacer> racers = Grid();
    std::array<float, 4> speeds{2.0F, 2.0F, 2.0F, 2.0F};

    band.Update(kDelay - kTick, racers, speeds);
    band.Update(kTick, racers, speeds);
    CHECK(speeds[1] == 2.0F);

    band.Update(Milliseconds{1}, racers, speeds);
    CHECK(speeds[1] == doctest::Approx(0.95F));
}

TEST_CASE("the rubber band slows racers ahead of the best player and speeds racers behind") {
    const std::array<float, 4> speeds = Paced(0.0F, Grid());

    CHECK(speeds[0] == 2.0F);
    CHECK(speeds[1] == doctest::Approx(0.95F));
    CHECK(speeds[2] == doctest::Approx(1.05F));
    CHECK(speeds[3] == 2.0F);
}

TEST_CASE("the rubber band adds its bonus to every paced racer") {
    const std::array<float, 4> speeds = Paced(0.05F, Grid());

    CHECK(speeds[1] == doctest::Approx(1.0F));
    CHECK(speeds[2] == doctest::Approx(1.1F));
}

TEST_CASE("the rubber band paces against the best of several players") {
    std::vector<PacedRacer> racers = Grid();
    racers[3] = {.lapProgress = 2.0F, .isAiControlled = false};

    const std::array<float, 4> speeds = Paced(0.0F, racers);

    CHECK(speeds[1] == doctest::Approx(1.05F));
    CHECK(speeds[3] == 2.0F);
}

TEST_CASE(
    "mirrored races pace 5 percent faster, and 2001 paces the first circuit 3 percent slower") {
    const VersionRules original;
    const VersionRules& rerelease = RulesFor(assets::GameVersion::Release2001);

    CHECK(PaceBonus(true, "c0", original) == doctest::Approx(0.05F));
    CHECK(PaceBonus(false, "c0", original) == 0.0F);
    CHECK(PaceBonus(true, "c0", rerelease) == doctest::Approx(0.05F));
    CHECK(PaceBonus(false, "c0", rerelease) == doctest::Approx(-0.03F));
    CHECK(PaceBonus(false, "c1", rerelease) == 0.0F);
}

TEST_CASE("with no player the rubber band sets every racer back to 1") {
    std::vector<PacedRacer> racers = Grid();
    racers[0].isAiControlled = true;

    const std::array<float, 4> speeds = Paced(0.05F, racers);

    CHECK(speeds == std::array{1.0F, 1.0F, 1.0F, 1.0F});
}

}
}
