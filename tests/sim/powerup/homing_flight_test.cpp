#include "sim/powerup/homing_flight.hpp"

#include <doctest/doctest.h>

#include <utility>
#include <vector>

#include "../physics/plane_world.hpp"
#include "assets/checkpoints.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};

CheckpointGraph TwoGates() {
    std::vector<assets::Checkpoint> gates{
        {.normal = {.x = -1.0F},
         .distance = 100.0F,
         .position = {.x = 100.0F},
         .links = {1, assets::kNoLink, assets::kNoLink, assets::kNoLink}},
        {.normal = {.x = -1.0F},
         .distance = 200.0F,
         .position = {.x = 200.0F, .y = 50.0F},
         .links = {0, assets::kNoLink, assets::kNoLink, assets::kNoLink}},
    };
    Result<CheckpointGraph> graph = CheckpointGraph::Create({.checkpoints = std::move(gates)});
    REQUIRE(graph.has_value());
    return std::move(*graph);
}

float SideAfterTicks(float startX, int ticks) {
    const CheckpointGraph graph = TwoGates();
    const test::PlaneWorld world = test::FlatGround();
    HomingFlight flight;
    flight.Launch({.start = {.x = startX, .z = 6.0F},
                   .direction = {.x = 1.0F},
                   .speed = 170.0F,
                   .snapSpeed = 170.0F,
                   .lifetime = Milliseconds{5500},
                   .pathGate = 0});
    for (int tick = 0; tick < ticks; ++tick) {
        flight.Steer(kTick, {}, 0, graph, world);
        REQUIRE(flight.Step(kTick, {}, world) == FlightState::Flying);
    }
    return flight.Direction().y;
}

TEST_CASE("a missile less than 10 units before its gate steers for the gate after it") {
    CHECK(SideAfterTicks(85.0F, 1) == doctest::Approx(0.0F));
    CHECK(SideAfterTicks(95.0F, 1) > 0.0F);
}

}
}
