#pragma once

#include <doctest/doctest.h>

#include <cstdint>
#include <utility>

#include "assets/checkpoints.hpp"
#include "core/result.hpp"
#include "sim/race/checkpoint_graph.hpp"

namespace rr::test {

inline constexpr std::uint8_t kNo = assets::kNoLink;

[[nodiscard]] inline assets::Checkpoint GateAt(float x, std::array<std::uint8_t, 4> links,
                                               float y = 0.0F) {
    return {.normal = {.x = -1.0F}, .distance = x, .position = {.x = x, .y = y}, .links = links};
}

/// Gates at X 0, 10, 20 and 30 in a loop, with a branch from gate 1 through gate 4 to gate 3.
[[nodiscard]] inline sim::CheckpointGraph Ring() {
    Result<sim::CheckpointGraph> graph =
        sim::CheckpointGraph::Create({.checkpoints = {
                                          GateAt(0.0F, {1, kNo, kNo, kNo}),
                                          GateAt(10.0F, {2, 4, kNo, kNo}),
                                          GateAt(20.0F, {3, kNo, kNo, kNo}),
                                          GateAt(30.0F, {0, kNo, kNo, kNo}),
                                          GateAt(20.0F, {3, kNo, kNo, kNo}, 500.0F),
                                      }});
    REQUIRE(graph.has_value());
    return std::move(*graph);
}

}
