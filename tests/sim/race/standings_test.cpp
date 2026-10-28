#include "sim/race/standings.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <vector>

namespace rr::sim {
namespace {

using Order = std::vector<std::size_t>;

Order Rank(const std::vector<RacerStanding>& racers) {
    Order order(racers.size());
    RankRacers(racers, order);
    return order;
}

TEST_CASE("racers rank by progress, highest first") {
    const std::vector<RacerStanding> racers{
        {.progress = 0.5F}, {.progress = 1.25F}, {.progress = 0.75F}};

    CHECK(Rank(racers) == Order{1, 2, 0});
}

TEST_CASE("a racer a lap down ranks behind, even further round its own lap") {
    const std::vector<RacerStanding> racers{
        {.progress = 0.9F}, {.progress = 1.1F}, {.progress = 2.05F}};

    CHECK(Rank(racers) == Order{2, 1, 0});
}

TEST_CASE("equal progress goes to the racer nearer its next gate") {
    const std::vector<RacerStanding> racers{{.progress = 1.5F, .distanceToNextGate = 30.0F},
                                            {.progress = 1.5F, .distanceToNextGate = 4.0F},
                                            {.progress = 1.5F, .distanceToNextGate = 12.0F},
                                            {.progress = 0.5F, .distanceToNextGate = 1.0F}};

    CHECK(Rank(racers) == Order{1, 2, 0, 3});
}

TEST_CASE("full ties keep racer order") {
    const std::vector<RacerStanding> racers{{.progress = 1.0F, .distanceToNextGate = 5.0F},
                                            {.progress = 1.0F, .distanceToNextGate = 5.0F}};

    CHECK(Rank(racers) == Order{0, 1});
}

TEST_CASE("finishers lead in finishing order whatever their progress") {
    const std::vector<RacerStanding> racers{{.progress = 3.4F},
                                            {.progress = 3.1F, .finishPlace = 2},
                                            {.progress = 3.0F, .finishPlace = 1},
                                            {.progress = 2.9F}};

    CHECK(Rank(racers) == Order{2, 1, 0, 3});
}

}
}
