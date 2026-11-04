#include "assets/gltf_scene.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

#include "assets/gltf_data.hpp"

namespace rr::assets {
namespace {

GeometryAsset Triangle(std::string_view material) {
    return {.vertices = {{.normal = {.z = 1.0F}},
                         {.position = {.x = 1.0F}, .normal = {.z = 1.0F}},
                         {.position = {.y = 1.0F}, .normal = {.z = 1.0F}}},
            .indices = {0, 1, 2},
            .materials = {std::string(material)},
            .ranges = {{.indexCount = 3}}};
}

std::vector<std::byte> SceneWithLine() {
    GltfScene scene;
    const std::array<Vector3, 3> points{Vector3{}, Vector3{.x = 1.0F},
                                        Vector3{.x = 1.0F, .y = 1.0F}};
    const std::size_t line = scene.AddLine(points);
    const std::size_t route = scene.AddNode({.name = "route", .mesh = line});
    const std::size_t root = scene.AddNode({.name = "routes", .children = {route}});
    const std::array roots{root};
    return scene.Build(roots);
}

TEST_CASE("a scene shares materials by name and places nodes in glTF axes") {
    GltfScene scene;
    const std::size_t first = scene.AddMesh(Triangle("ROAD"));
    const std::size_t second = scene.AddMesh(Triangle("ROAD"));
    const std::array roots{
        scene.AddNode({.name = "a", .position = {.x = 1.0F, .y = 2.0F, .z = 3.0F}, .mesh = first}),
        scene.AddNode({.name = "b", .mesh = second, .extras = R"({"links":["gate.02"]})"})};

    const Result<GltfData> gltf = ParseGlb(scene.Build(roots));

    REQUIRE(gltf.has_value());
    REQUIRE((*gltf)->nodes_count == 2);
    CHECK((*gltf)->materials_count == 1);
    const std::array<float, 3> translation{(*gltf)->nodes[0].translation[0],
                                           (*gltf)->nodes[0].translation[1],
                                           (*gltf)->nodes[0].translation[2]};
    CHECK(translation == std::array{1.0F, 3.0F, -2.0F});
    CHECK(std::string_view((*gltf)->nodes[1].extras.data) == R"({"links":["gate.02"]})");
}

TEST_CASE("a scene line is a line strip under its parent") {
    const Result<GltfData> gltf = ParseGlb(SceneWithLine());

    REQUIRE(gltf.has_value());
    REQUIRE((*gltf)->meshes_count == 1);
    CHECK((*gltf)->meshes[0].primitives[0].type == cgltf_primitive_type_line_strip);
    CHECK((*gltf)->meshes[0].primitives[0].attributes[0].data->count == 3);
    CHECK((*gltf)->nodes[1].children_count == 1);
}

}
}
