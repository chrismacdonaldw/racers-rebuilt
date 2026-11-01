#include "sim/race/circuit_run.hpp"

#include <doctest/doctest.h>

#include <array>
#include <optional>

namespace rr::sim {
namespace {

constexpr std::array kFourRaces{true, true, true, true};
using Places = std::array<std::optional<int>, assets::kGridSlots>;

constexpr Places kPlayerWins{1, 2, 3, 4, 5, 6};
constexpr Places kPlayerSecond{2, 1, 3, 4, 5, 6};
constexpr Places kPlayerLast{6, 1, 2, 3, 4, 5};

TEST_CASE("finishing places score 30, 20, 10, 3, 2 and 1") {
    CHECK(PlacePoints(1) == 30);
    CHECK(PlacePoints(2) == 20);
    CHECK(PlacePoints(3) == 10);
    CHECK(PlacePoints(4) == 3);
    CHECK(PlacePoints(5) == 2);
    CHECK(PlacePoints(6) == 1);
    CHECK(PlacePoints(7) == 0);
    CHECK(PlacePoints(std::nullopt) == 0);
}

TEST_CASE("a circuit runs its races in order and adds each race's points") {
    CircuitRun run(kFourRaces, 0, true);

    CHECK(run.Race() == 0);
    run.FinishRace(kPlayerWins);
    CHECK(run.Race() == 1);
    run.FinishRace(kPlayerSecond);

    CHECK(run.Race() == 2);
    CHECK(run.Points()[0] == 50);
    CHECK(run.Points()[1] == 50);
    CHECK(run.Points()[5] == 2);
}

TEST_CASE("winning every race wins gold") {
    CircuitRun run(kFourRaces, 0, true);

    for (int race = 0; race < 4; ++race) {
        run.FinishRace(kPlayerWins);
    }

    CHECK_FALSE(run.Race().has_value());
    CHECK(run.Award() == CircuitAward::Gold);
}

TEST_CASE("the player's rank on total points picks the award, ties sharing the better rank") {
    CircuitRun tied(kFourRaces, 0, true);
    tied.FinishRace(kPlayerWins);
    tied.FinishRace(kPlayerSecond);
    tied.FinishRace(kPlayerWins);
    tied.FinishRace(kPlayerSecond);
    CHECK(tied.Rank(0) == 0);
    CHECK(tied.Award() == CircuitAward::Gold);

    CircuitRun second(kFourRaces, 0, true);
    for (int race = 0; race < 4; ++race) {
        second.FinishRace(kPlayerSecond);
    }
    CHECK(second.Award() == CircuitAward::Silver);

    CircuitRun third(kFourRaces, 0, true);
    for (int race = 0; race < 4; ++race) {
        third.FinishRace(Places{3, 1, 2, 4, 5, 6});
    }
    CHECK(third.Award() == CircuitAward::Bronze);
}

TEST_CASE("a lone player under 10 points per race run is knocked out before the next race") {
    CircuitRun run(kFourRaces, 0, true);
    run.FinishRace(Places{4, 1, 2, 3, 5, 6});
    CHECK_FALSE(run.Race().has_value());
    CHECK(run.Award() == CircuitAward::None);

    CircuitRun scraping(kFourRaces, 0, true);
    scraping.FinishRace(Places{3, 1, 2, 4, 5, 6});
    scraping.FinishRace(Places{3, 1, 2, 4, 5, 6});
    CHECK(scraping.Race() == 2);
    scraping.FinishRace(Places{6, 1, 2, 3, 4, 5});
    CHECK(scraping.Award() == CircuitAward::None);
}

TEST_CASE("with more than one player nobody is knocked out") {
    CircuitRun run(kFourRaces, 0, false);

    for (int race = 0; race < 3; ++race) {
        run.FinishRace(kPlayerLast);
        CHECK(run.Race() == static_cast<std::size_t>(race + 1));
    }
}

TEST_CASE("empty race slots are skipped, and a one-race circuit ends after it") {
    CircuitRun gaps({true, false, true, false}, 0, true);
    gaps.FinishRace(kPlayerWins);
    CHECK(gaps.Race() == 2);
    gaps.FinishRace(kPlayerWins);
    CHECK(gaps.Award() == CircuitAward::Gold);

    CircuitRun single({true, false, false, false}, 0, true);
    single.FinishRace(kPlayerLast);
    CHECK(single.Award() == CircuitAward::None);
}

TEST_CASE("a restart clears every point and starts again from the first race") {
    CircuitRun run(kFourRaces, 0, true);
    run.FinishRace(kPlayerWins);
    run.FinishRace(kPlayerWins);

    run.Restart();

    CHECK(run.Race() == 0);
    CHECK(run.Points()[0] == 0);
    CHECK_FALSE(run.Award().has_value());
}

}
}
