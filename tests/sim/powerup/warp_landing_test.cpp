#include "sim/powerup/warp_landing.hpp"

#include <doctest/doctest.h>

#include <utility>
#include <vector>

#include "../physics/plane_world.hpp"

namespace rr::sim {
namespace {

using assets::kNoLink;

constexpr Vector3 kBeforeCorner{.x = 100.0F, .y = 20.0F};

CheckpointGraph Corner() {
    std::vector<assets::Checkpoint> gates{
        {.normal = {.x = -1.0F}, .links = {1, kNoLink, kNoLink, kNoLink}},
        {.normal = {.x = -1.0F},
         .position = {.x = 100.0F, .y = 100.0F},
         .links = {2, kNoLink, kNoLink, kNoLink}},
        {.normal = {.x = 1.0F},
         .position = {.y = 100.0F},
         .links = {0, kNoLink, kNoLink, kNoLink}}};
    Result<CheckpointGraph> graph = CheckpointGraph::Create({.checkpoints = std::move(gates)});
    REQUIRE(graph.has_value());
    return std::move(*graph);
}

}

TEST_CASE("a landing faces along the gate walk, not the next gate's normal") {
    const CheckpointGraph graph = Corner();
    const Vector3 heading = ClearLandingHeading(test::FlatGround(), graph, kBeforeCorner, 1);
    CHECK(heading.x == doctest::Approx(0.0F));
    CHECK(heading.y == doctest::Approx(1.0F));
    CHECK(heading.z == 0.0F);
}

TEST_CASE("a landing walking into a wall turns to the nearest clear heading, left first") {
    const CheckpointGraph graph = Corner();
    const test::PlaneWorld world(
        {test::TestPlane{}, test::TestPlane{.normal = {.y = -1.0F}, .offset = -60.0F}});
    const Vector3 heading = ClearLandingHeading(world, graph, kBeforeCorner, 1);
    CHECK(heading.x == doctest::Approx(-0.866F).epsilon(0.001));
    CHECK(heading.y == doctest::Approx(0.5F));
}

TEST_CASE("a landing with a wall all round keeps the heading that runs furthest") {
    const CheckpointGraph graph = Corner();
    const test::PlaneWorld world(
        {test::TestPlane{}, test::TestPlane{.normal = {.y = -1.0F}, .offset = -30.0F}});
    const Vector3 heading = ClearLandingHeading(world, graph, kBeforeCorner, 1);
    CHECK(heading.x == doctest::Approx(-0.966F).epsilon(0.001));
    CHECK(heading.y == doctest::Approx(0.259F).epsilon(0.002));
}

TEST_CASE("ground ahead doesn't count as a wall") {
    const CheckpointGraph graph = Corner();
    const test::PlaneWorld world(
        {test::TestPlane{.normal = Normalized({.y = -1.0F, .z = 4.0F}, {}), .offset = 0.0F}});
    const Vector3 heading =
        ClearLandingHeading(world, graph, kBeforeCorner + Vector3{.z = 6.0F}, 1);
    CHECK(heading.y == doctest::Approx(1.0F));
}

}
