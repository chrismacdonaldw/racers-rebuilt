#include "sim/race/checkpoint_graph.hpp"

#include <doctest/doctest.h>

#include <optional>

#include "checkpoint_ring.hpp"

namespace rr::sim {
namespace {

using test::GateAt;
using test::kNo;

TEST_CASE("the main loop splits a lap evenly and branches fill in between") {
    const CheckpointGraph graph = test::Ring();

    CHECK(graph.LapFraction(0) == 0.0F);
    CHECK(graph.LapFraction(1) == 0.25F);
    CHECK(graph.LapFraction(2) == 0.5F);
    CHECK(graph.LapFraction(3) == 0.75F);
    CHECK(graph.LapFraction(4) == 0.5F);
}

TEST_CASE("a checkpoint graph needs a loop through checkpoint 0") {
    const Result<CheckpointGraph> open =
        CheckpointGraph::Create({.checkpoints = {
                                     GateAt(0.0F, {1, kNo, kNo, kNo}),
                                     GateAt(10.0F, {1, kNo, kNo, kNo}),
                                 }});

    CHECK_FALSE(open.has_value());
}

TEST_CASE("a checkpoint graph rejects branches that never rejoin and unreachable checkpoints") {
    const Result<CheckpointGraph> stray =
        CheckpointGraph::Create({.checkpoints = {
                                     GateAt(0.0F, {0, 1, kNo, kNo}),
                                     GateAt(10.0F, {2, kNo, kNo, kNo}),
                                     GateAt(20.0F, {1, kNo, kNo, kNo}),
                                 }});
    const Result<CheckpointGraph> unreachable =
        CheckpointGraph::Create({.checkpoints = {
                                     GateAt(0.0F, {0, kNo, kNo, kNo}),
                                     GateAt(10.0F, {0, kNo, kNo, kNo}),
                                 }});

    CHECK_FALSE(stray.has_value());
    CHECK_FALSE(unreachable.has_value());
}

TEST_CASE("crossing a gate with travel is forward and against it is backward") {
    const CheckpointGraph graph = test::Ring();

    CHECK(graph.FindCrossing({.x = 8.0F}, {.x = 12.0F}) ==
          GateCrossing{.checkpoint = 1, .side = GateSide::Forward});
    CHECK(graph.FindCrossing({.x = 12.0F}, {.x = 8.0F}) ==
          GateCrossing{.checkpoint = 1, .side = GateSide::Backward});
    CHECK_FALSE(graph.FindCrossing({.x = 11.0F}, {.x = 12.0F}).has_value());
}

TEST_CASE("a gate only spans its radius and the first gate along the way wins") {
    const CheckpointGraph graph = test::Ring();
    const float wide = kGateRadius + 1.0F;

    CHECK_FALSE(graph.FindCrossing({.x = 8.0F, .y = wide}, {.x = 12.0F, .y = wide}).has_value());
    CHECK(graph.FindCrossing({.x = 5.0F}, {.x = 25.0F}) == GateCrossing{.checkpoint = 1});
    CHECK(graph.FindCrossing({.x = 18.0F, .y = 500.0F}, {.x = 22.0F, .y = 500.0F}) ==
          GateCrossing{.checkpoint = 4});
}

TEST_CASE("the distance to the next gate takes the nearest linked gate") {
    const CheckpointGraph graph = test::Ring();

    CHECK(graph.DistanceToNextGate(0, {.x = 4.0F}) == 6.0F);
    CHECK(graph.DistanceToNextGate(1, {.x = 12.0F}) == 8.0F);
}

}
}
