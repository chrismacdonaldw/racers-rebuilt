#include "assets/gltf_model.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "assets/geometry_database.hpp"
#include "assets/geometry_writer.hpp"
#include "gltf_files.hpp"

namespace rr::assets {
namespace {

GltfModel Import(const test::GlbTriangle& triangle) {
    Result<GltfModel> model = ImportGltfModel(test::BuildGlb(triangle));
    REQUIRE(model.has_value());
    return std::move(*model);
}

TEST_CASE("a glTF mesh is moved by its nodes and turned into game axes") {
    const GltfModel model = Import({.translation = {1.0F, 2.0F, 3.0F}});
    const GeometryAsset& geometry = model.geometry;

    CHECK(model.vertices == GdbVertices::Lit);
    REQUIRE(geometry.vertices.size() == 3);
    CHECK(geometry.vertices[0].position == Vector3{.x = 1.0F, .y = -3.0F, .z = 2.0F});
    CHECK(geometry.vertices[1].position == Vector3{.x = 2.0F, .y = -3.0F, .z = 2.0F});
    CHECK(geometry.vertices[2].position == Vector3{.x = 1.0F, .y = -3.0F, .z = 3.0F});
    CHECK(geometry.vertices[1].normal == Vector3{.x = 0.0F, .y = -1.0F, .z = 0.0F});
    CHECK(geometry.vertices[2].u == 0.0F);
    CHECK(geometry.vertices[2].v == 1.0F);
    CHECK(geometry.indices == std::vector<std::uint32_t>{0, 1, 2});
    CHECK(geometry.materials == std::vector<std::string>{"STONE"});
    REQUIRE(geometry.ranges.size() == 1);
    CHECK(geometry.ranges[0].indexCount == 3);
}

TEST_CASE("linear glTF vertex colours become the game's gamma colours") {
    const GltfModel model = Import({.hasNormals = false, .hasColors = true});
    const GeometryAsset& geometry = model.geometry;

    CHECK(model.vertices == GdbVertices::Colored);
    REQUIRE(geometry.vertices.size() == 3);
    CHECK(geometry.vertices[0].rgba == Rgba8{255, 0, 0, 255});
    CHECK(geometry.vertices[1].rgba == Rgba8{0, 255, 0, 128});
    CHECK(geometry.vertices[2].rgba == Rgba8{188, 188, 188, 255});
}

TEST_CASE("an imported model writes a GDB that reads back the same") {
    const GltfModel model = Import({.translation = {0.5F, 0.0F, -2.0F}});

    const Result<GeometryAsset> back =
        WriteGeometry(model.geometry, model.vertices)
            .and_then([](const std::vector<std::byte>& bytes) { return ParseGeometry(bytes); });

    REQUIRE(back.has_value());
    CHECK(back->materials == model.geometry.materials);
    REQUIRE(back->vertices.size() == model.geometry.vertices.size());
    CHECK(back->vertices[1].position == model.geometry.vertices[1].position);
}

TEST_CASE("models the game can't draw are rejected") {
    CHECK_FALSE(ImportGltfModel(test::BuildGlb({.mode = test::kGltfPoints})).has_value());
    CHECK_FALSE(ImportGltfModel(test::BuildGlb({.material = ""})).has_value());
    CHECK_FALSE(ImportGltfModel(test::BuildGlb({.hasNormals = false})).has_value());
    CHECK_FALSE(ImportGltfModel(test::BuildGlb({.lastIndex = 3})).has_value());
    const std::vector<std::byte> junk(16, std::byte{0x42});
    CHECK_FALSE(ImportGltfModel(junk).has_value());
}

TEST_CASE("an attribute with the wrong type is rejected before it's read") {
    const Result<GltfModel> model = ImportGltfModel(test::BuildGlb({.normalType = "VEC2"}));

    REQUIRE_FALSE(model.has_value());
    CHECK(model.error().message.contains("wrong type"));
}

}
}
