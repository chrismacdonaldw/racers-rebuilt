#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "assets/collision_mesh.hpp"
#include "core/vector3.hpp"

namespace rr::test {

/// A 20 × 20 floor at z = 0 around the origin, and a wall at x = 5 facing -x, 10 high.
[[nodiscard]] inline assets::CollisionMeshAsset FloorAndWall(std::vector<std::string> materials = {
                                                                 "road", "wall"}) {
    return {
        .materials = std::move(materials),
        .vertices = {{.x = -10.0F, .y = -10.0F},
                     {.x = 10.0F, .y = -10.0F},
                     {.x = 10.0F, .y = 10.0F},
                     {.x = -10.0F, .y = 10.0F},
                     {.x = 5.0F, .y = -10.0F},
                     {.x = 5.0F, .y = 10.0F},
                     {.x = 5.0F, .y = 10.0F, .z = 10.0F},
                     {.x = 5.0F, .y = -10.0F, .z = 10.0F}},
        .triangles = {{.vertices = {0, 1, 2}, .material = 0},
                      {.vertices = {0, 2, 3}, .material = 0},
                      {.vertices = {4, 6, 5}, .material = 1},
                      {.vertices = {4, 7, 6}, .material = 1}},
        .nodes = {{.front = 1, .normal = {.z = 1.0F}, .firstTriangle = 0, .triangleCount = 2},
                  {.normal = {.x = -1.0F}, .firstTriangle = 2, .triangleCount = 2}},
    };
}

/// `depth` nodes, each the front child of the one before, all on the floor's first triangle.
[[nodiscard]] inline assets::CollisionMeshAsset Chain(std::int32_t depth) {
    assets::CollisionMeshAsset mesh = FloorAndWall();
    mesh.nodes.clear();
    for (std::int32_t i = 0; i < depth; ++i) {
        mesh.nodes.push_back({.front = i + 1 < depth ? i + 1 : assets::kNoChild,
                              .normal = {.z = 1.0F},
                              .triangleCount = 1});
    }
    return mesh;
}

}
