#include "assets/track_markers.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "assets/gltf_scene.hpp"

namespace rr::assets {
namespace {

TrackMarkers MarkersOf(std::vector<GltfNode> nodes,
                       std::span<const Vector3> line = std::span<const Vector3>()) {
    GltfScene scene;
    std::vector<std::size_t> roots;
    roots.reserve(nodes.size());
    for (GltfNode& node : nodes) {
        if (node.name.starts_with("route") && !line.empty()) {
            node.mesh = scene.AddLine(line);
        }
        roots.push_back(scene.AddNode(std::move(node)));
    }
    Result<TrackMarkers> markers = ParseTrackMarkers(scene.Build(roots));
    REQUIRE(markers.has_value());
    return std::move(*markers);
}

TEST_CASE("gates without links lead to the next and the last back to the first") {
    const TrackMarkers markers = MarkersOf(
        {{.name = "gate.02", .position = {.x = 10.0F}}, {.name = "gate.01"}, {.name = "world"}});

    const std::vector<Checkpoint> gates = markers.gates.value_or(CheckpointsAsset{}).checkpoints;
    REQUIRE(gates.size() == 2);
    CHECK(gates[0].links[0] == 1);
    CHECK(gates[1].links[0] == 0);
    CHECK(gates[1].normal == Vector3{.x = -1.0F});
    CHECK(markers.problems.empty());
}

TEST_CASE("bricks and hazards collect by kind, and unknown kinds are problems") {
    const TrackMarkers markers = MarkersOf({{.name = "brick.red.1"},
                                            {.name = "brick.white.1.001"},
                                            {.name = "brick.plaid.1"},
                                            {.name = "hazard.cannon.1", .position = {.z = 2.0F}},
                                            {.name = "hazard.volcano.1"}});

    const BrickLayout bricks = markers.bricks.value_or(BrickLayout{});
    CHECK(bricks.coloured.size() == 1);
    CHECK(bricks.white.size() == 1);
    const std::vector<Hazard> hazards = markers.hazards.value_or(std::vector<Hazard>{});
    REQUIRE(hazards.size() == 1);
    CHECK(std::get<CannonShot>(hazards.front().payload).launch == Vector3{.z = 2.0F});
    CHECK(markers.problems ==
          std::vector<std::string>{"brick.plaid.1: there's no brick colour plaid",
                                   "hazard.volcano.1: there's no hazard kind volcano"});
}

TEST_CASE("a short grid, a gap in the gates and an untimed route are problems") {
    const std::array line{Vector3{}, Vector3{.x = 1.0F}};

    const TrackMarkers markers = MarkersOf({{.name = "start.1"},
                                            {.name = "start.2"},
                                            {.name = "gate.01"},
                                            {.name = "gate.03"},
                                            {.name = "route.1.F.0"}},
                                           line);

    CHECK(markers.grid.value_or(StartGridAsset{}).slots.size() == 2);
    CHECK(markers.gates.value_or(CheckpointsAsset{}).checkpoints.size() == 1);
    CHECK(markers.routes.empty());
    CHECK(markers.problems.size() == 3);
}

TEST_CASE("a timed route reads its points as offsets from the start") {
    const std::array line{Vector3{.y = 1.0F}, Vector3{.x = 2.0F, .y = 1.0F}};

    const TrackMarkers markers =
        MarkersOf({{.name = "route.2.M.0", .extras = R"({"durations_ms":[500]})"}}, line);

    REQUIRE(markers.routes.size() == 1);
    CHECK(markers.routes[0].slot == 2);
    CHECK(markers.routes[0].route.pace == RoutePace::Medium);
    CHECK(markers.routes[0].route.points[0].offset == Vector3{.x = 2.0F});
    CHECK(markers.routes[0].route.loopTimeMs == 500);
}

}
}
