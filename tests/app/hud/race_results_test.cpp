#include "app/hud/race_results.hpp"

#include <doctest/doctest.h>

#include <array>
#include <vector>

#include "ring_race.hpp"

namespace rr::app {
namespace {

TEST_CASE("results time each finisher against the winner and mark racers still out") {
    test::RingRace race(3, 1);
    race.StartRacing();
    race.DriveLap(2);
    race.DriveLap(2);
    race.DriveLap(0);
    race.Wait(0, Milliseconds{1250});
    race.DriveLap(0);
    race.Step(1, -5.0F, 5.0F);
    const std::array<Vector3, 3> positions{Vector3{.x = -5.0F}, Vector3{.x = 5.0F},
                                           Vector3{.x = -5.0F}};
    race.progress.Rank(race.graph, positions);
    std::vector<ResultRow> rows{{.racer = 9}};

    ReadResults(race.progress, rows);

    REQUIRE(rows.size() == 3);
    CHECK(rows[0].racer == 2);
    CHECK(rows[0].status == ResultStatus::Winner);
    CHECK(rows[1].racer == 0);
    CHECK(rows[1].status == ResultStatus::Behind);
    CHECK(rows[1].behind == Milliseconds{1250});
    CHECK(rows[2].racer == 1);
    CHECK(rows[2].status == ResultStatus::Racing);
}

TEST_CASE("racers given places without finishing still show as racing") {
    test::RingRace race(2, 1);
    race.StartRacing();
    race.DriveLap(1);
    race.DriveLap(1);
    race.progress.FinishRemaining();
    const std::array<Vector3, 2> positions{};
    race.progress.Rank(race.graph, positions);
    std::vector<ResultRow> rows;

    ReadResults(race.progress, rows);

    REQUIRE(rows.size() == 2);
    CHECK(rows[0].racer == 1);
    CHECK(rows[1].status == ResultStatus::Racing);
}

}
}
