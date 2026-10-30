#include "sim/race/race_progress.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "checkpoint_ring.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kStep{100};
constexpr Vector3 kAlongTravel{.x = 1.0F};
constexpr float kOffRouteY = 200.0F;

void Step(RaceProgress& progress, const CheckpointGraph& graph, std::size_t racer, float fromX,
          float toX, float y = 0.0F) {
    progress.Move(graph, racer,
                  {.from = {.x = fromX, .y = y}, .to = {.x = toX, .y = y}, .facing = kAlongTravel},
                  kStep, true);
}

void DriveLap(RaceProgress& progress, const CheckpointGraph& graph, std::size_t racer) {
    for (const float x : {-5.0F, 5.0F, 15.0F, 25.0F}) {
        Step(progress, graph, racer, x, x + 10.0F);
    }
    Step(progress, graph, racer, 35.0F, -5.0F, kOffRouteY);
}

TEST_CASE("each lap's time goes to that lap") {
    const CheckpointGraph graph = test::Ring();
    RaceProgress progress(1, 3);

    Step(progress, graph, 0, -15.0F, -5.0F);
    DriveLap(progress, graph, 0);
    DriveLap(progress, graph, 0);

    const RacerProgress& racer = progress.OfRacer(0);
    CHECK(racer.laps.CompletedLaps() == 1);
    CHECK(racer.lapTimes ==
          std::vector<Milliseconds>{Milliseconds{600}, Milliseconds{500}, Milliseconds{0}});
}

TEST_CASE("time outside the running phases counts toward no lap") {
    const CheckpointGraph graph = test::Ring();
    RaceProgress progress(1, 3);

    progress.Move(graph, 0, {.from = {.x = -15.0F}, .to = {.x = -5.0F}, .facing = kAlongTravel},
                  kStep, false);

    CHECK(progress.OfRacer(0).lapTimes[0] == Milliseconds{0});
}

TEST_CASE("racers finish in the order they reach the lap count and their times stop") {
    const CheckpointGraph graph = test::Ring();
    RaceProgress progress(3, 1);

    DriveLap(progress, graph, 2);
    DriveLap(progress, graph, 2);
    DriveLap(progress, graph, 0);
    DriveLap(progress, graph, 0);
    const std::vector<Milliseconds> finishedTimes = progress.OfRacer(2).lapTimes;
    DriveLap(progress, graph, 2);

    CHECK(progress.OfRacer(2).finishPlace == std::optional<int>{1});
    CHECK(progress.OfRacer(0).finishPlace == std::optional<int>{2});
    CHECK_FALSE(progress.OfRacer(1).finishPlace.has_value());
    CHECK(progress.OfRacer(2).lapTimes == finishedTimes);
}

TEST_CASE("unfinished racers take the remaining places in racer order") {
    const CheckpointGraph graph = test::Ring();
    RaceProgress progress(3, 1);
    DriveLap(progress, graph, 1);
    DriveLap(progress, graph, 1);
    Step(progress, graph, 2, -5.0F, 5.0F);

    progress.FinishRemaining();

    CHECK(progress.OfRacer(1).finishPlace == std::optional<int>{1});
    CHECK(progress.OfRacer(0).finishPlace == std::optional<int>{2});
    CHECK(progress.OfRacer(2).finishPlace == std::optional<int>{3});
}

TEST_CASE("standings put the racer furthest round first") {
    const CheckpointGraph graph = test::Ring();
    RaceProgress progress(3, 2);
    Step(progress, graph, 0, -5.0F, 5.0F);
    Step(progress, graph, 1, -5.0F, 5.0F);
    Step(progress, graph, 1, 5.0F, 15.0F);
    const std::array<Vector3, 3> positions{Vector3{.x = 5.0F}, Vector3{.x = 15.0F},
                                           Vector3{.x = -8.0F}};

    progress.Rank(graph, positions);

    CHECK(std::vector(progress.Standings().begin(), progress.Standings().end()) ==
          std::vector<std::size_t>{1, 0, 2});
}

TEST_CASE("before any gate, the racer nearer checkpoint 0 ranks ahead") {
    const CheckpointGraph graph = test::Ring();
    RaceProgress progress(2, 3);
    const std::array<Vector3, 2> positions{Vector3{.x = -20.0F}, Vector3{.x = -10.0F}};

    progress.Rank(graph, positions);

    CHECK(progress.Standings().front() == 1);
}

}
}
