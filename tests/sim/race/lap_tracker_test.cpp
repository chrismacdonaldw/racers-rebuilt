#include "sim/race/lap_tracker.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <initializer_list>

#include "checkpoint_ring.hpp"

namespace rr::sim {
namespace {

constexpr Vector3 kAlongTravel{.x = 1.0F};
constexpr Vector3 kAgainstTravel{.x = -1.0F};

void CrossAll(LapTracker& laps, const CheckpointGraph& graph,
              std::initializer_list<std::size_t> gates, GateSide side = GateSide::Forward) {
    for (const std::size_t gate : gates) {
        laps.Cross(graph, {.checkpoint = gate, .side = side});
    }
}

void ShuttleOverLine(LapTracker& laps, const CheckpointGraph& graph) {
    CrossAll(laps, graph, {0});
    laps.EnterZone(assets::LapZone::PostLine);
    laps.EnterZone(assets::LapZone::OpenTrack);
    laps.EnterZone(assets::LapZone::PostLine);
    CrossAll(laps, graph, {0}, GateSide::Backward);
    laps.EnterZone(assets::LapZone::FinishLine);
}

void Drive(LapTracker& laps, const CheckpointGraph& graph, const Vector3& facing, int ms) {
    for (int elapsed = 0; elapsed < ms; elapsed += 12) {
        laps.Update(graph, Milliseconds{12}, facing);
    }
}

TEST_CASE("progress starts at 0 and leaving the grid starts lap 0") {
    const CheckpointGraph graph = test::Ring();
    LapTracker laps;
    CHECK(laps.Progress(graph) == 0.0F);

    CrossAll(laps, graph, {0, 1});

    CHECK(laps.Progress(graph) == 0.25F);
    CHECK(laps.CompletedLaps() == 0);
}

TEST_CASE("crossing checkpoint 0 again completes a lap, by the main loop or a branch") {
    const CheckpointGraph graph = test::Ring();
    LapTracker laps;

    CrossAll(laps, graph, {0, 1, 2, 3, 0, 1, 4, 3, 0});

    CHECK(laps.CompletedLaps() == 2);
    CHECK(laps.Progress(graph) == 2.0F);
}

TEST_CASE("crossing the same gate the same way twice counts once") {
    const CheckpointGraph graph = test::Ring();
    LapTracker laps;

    CrossAll(laps, graph, {0, 1, 2, 3, 0, 0});

    CHECK(laps.CompletedLaps() == 1);
}

TEST_CASE("backing over checkpoint 0 and driving on doesn't count another lap") {
    const CheckpointGraph graph = test::Ring();
    LapTracker laps;
    CrossAll(laps, graph, {0, 1, 2, 3, 0});

    CrossAll(laps, graph, {0}, GateSide::Backward);
    CrossAll(laps, graph, {0, 1});

    CHECK(laps.CompletedLaps() == 1);
    CHECK(laps.Progress(graph) == 1.25F);
}

TEST_CASE("backing further than checkpoint 0 returns to the previous lap") {
    const CheckpointGraph graph = test::Ring();
    LapTracker laps;
    CrossAll(laps, graph, {0, 1, 2, 3, 0});

    CrossAll(laps, graph, {0, 3}, GateSide::Backward);
    CHECK(laps.Progress(graph) == 0.75F);

    CrossAll(laps, graph, {3, 0});
    CHECK(laps.CompletedLaps() == 1);
    CHECK(laps.Progress(graph) == 1.0F);
}

TEST_CASE("with lap zones, checkpoint crossings complete no laps") {
    const CheckpointGraph graph = test::Ring();
    LapTracker laps({.usesZones = true});

    CrossAll(laps, graph, {0, 1, 2, 3, 0, 1});
    CHECK(laps.CompletedLaps() == 0);
}

TEST_CASE("with lap zones, a full lap of zones after the first line crossing completes a lap") {
    const CheckpointGraph graph = test::Ring();
    LapTracker laps({.usesZones = true});
    CrossAll(laps, graph, {0, 1, 2, 3, 0, 1});

    int lapsDone = 0;
    for (const assets::LapZone zone : {assets::LapZone::FinishLine, assets::LapZone::PostLine,
                                       assets::LapZone::OpenTrack, assets::LapZone::FinishLine}) {
        lapsDone += laps.EnterZone(zone) ? 1 : 0;
    }
    CHECK(lapsDone == 1);
    CHECK(laps.CompletedLaps() == 1);
    CHECK(laps.Progress(graph) == 1.25F);
}

TEST_CASE(
    "2001 counts a lap each time a racer backs over the line from the open zone, 1999 doesn't") {
    const CheckpointGraph graph = test::Ring();
    for (const bool keepsOneZone : {false, true}) {
        LapTracker laps({.usesZones = true, .keepsOneZone = keepsOneZone});
        laps.EnterZone(assets::LapZone::FinishLine);

        for (int shuttle = 0; shuttle < 3; ++shuttle) {
            ShuttleOverLine(laps, graph);
        }

        CHECK(laps.CompletedLaps() == (keepsOneZone ? 3 : 0));
    }
}

TEST_CASE("without lap zones, zone entries count nothing") {
    LapTracker laps;

    CHECK_FALSE(laps.EnterZone(assets::LapZone::FinishLine));
    CHECK(laps.CompletedLaps() == 0);
}

TEST_CASE("facing back past the last gate for 500 ms warns of the wrong way") {
    const CheckpointGraph graph = test::Ring();
    LapTracker laps;
    Drive(laps, graph, kAgainstTravel, 600);
    CHECK_FALSE(laps.IsWrongWay());

    CrossAll(laps, graph, {0});
    Drive(laps, graph, kAgainstTravel, 492);
    CHECK_FALSE(laps.IsWrongWay());
    Drive(laps, graph, kAgainstTravel, 24);
    CHECK(laps.IsWrongWay());
}

TEST_CASE("the wrong-way warning clears after 500 ms facing ahead") {
    const CheckpointGraph graph = test::Ring();
    LapTracker laps;
    CrossAll(laps, graph, {0});
    Drive(laps, graph, kAgainstTravel, 1200);

    Drive(laps, graph, kAlongTravel, 480);
    CHECK(laps.IsWrongWay());
    Drive(laps, graph, kAlongTravel, 24);
    CHECK_FALSE(laps.IsWrongWay());
}

TEST_CASE("turning back ahead within 500 ms never warns") {
    const CheckpointGraph graph = test::Ring();
    LapTracker laps;
    CrossAll(laps, graph, {0});

    for (int turn = 0; turn < 10; ++turn) {
        Drive(laps, graph, kAgainstTravel, 480);
        Drive(laps, graph, kAlongTravel, 12);
    }

    CHECK_FALSE(laps.IsWrongWay());
}

}
}
