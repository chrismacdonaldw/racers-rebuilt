#include "assets/gltf_joints.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "assets/geometry_database.hpp"
#include "assets/geometry_writer.hpp"
#include "assets/gltf_model.hpp"
#include "gltf_files.hpp"

namespace rr::assets {
namespace {

constexpr float kDriverScale = 1.0F / 64.0F;

GltfTarget ArmSkeleton() {
    return {.joints = {"ROOT", "ARM"}, .scale = kDriverScale};
}

GltfModel Import(const test::GlbTriangle& triangle, const GltfTarget& target) {
    Result<GltfModel> model = ImportGltfModel(test::BuildGlb(triangle), target);
    REQUIRE(model.has_value());
    return std::move(*model);
}

TEST_CASE("a mesh under a skeleton node is stored in that node's space with its joint") {
    const GltfModel model =
        Import({.translation = {1.0F, 2.0F, 3.0F}, .parentName = "Arm"}, ArmSkeleton());

    const Result<GeometryAsset> back =
        WriteGeometry(model.geometry, model.vertices)
            .and_then([](const std::vector<std::byte>& bytes) { return ParseGeometry(bytes); });

    REQUIRE(back.has_value());
    CHECK(back->scale == kDriverScale);
    REQUIRE(back->vertices.size() == 3);
    CHECK(back->vertices[1].position == Vector3{.x = 1.0F, .y = 0.0F, .z = 0.0F});
    CHECK(back->vertices[1].joint == 1);
}

TEST_CASE("a mesh under no skeleton node uses joint 0 and its world position") {
    const GltfModel model = Import({.translation = {1.0F, 2.0F, 3.0F}}, ArmSkeleton());

    REQUIRE(model.geometry.vertices.size() == 3);
    CHECK(model.geometry.vertices[1].position == Vector3{.x = 2.0F, .y = -3.0F, .z = 2.0F});
    CHECK(model.geometry.vertices[1].joint == 0);
}

TEST_CASE("a skinned vertex takes its heaviest joint") {
    const GltfModel model = Import({.parentName = "root", .isSkinned = true}, ArmSkeleton());
    const std::vector<GeometryVertex>& vertices = model.geometry.vertices;

    REQUIRE(vertices.size() == 3);
    CHECK(vertices[0].joint == 0);
    CHECK(vertices[1].joint == 0);
    CHECK(vertices[2].joint == 1);
    CHECK(vertices[2].position == Vector3{.x = 0.0F, .y = 0.0F, .z = 1.0F});
}

TEST_CASE("a skin joint missing from the skeleton fails the import") {
    const Result<GltfModel> model =
        ImportGltfModel(test::BuildGlb({.parentName = "root", .isSkinned = true}),
                        {.joints = {"ROOT"}, .scale = 1.0F});

    REQUIRE_FALSE(model.has_value());
    CHECK(model.error().message.contains("isn't in the model's skeleton"));
}

TEST_CASE("a zero normal points up instead of failing the import") {
    const GltfModel model = Import({.normalZ = 0.0F}, {});

    REQUIRE(model.geometry.vertices.size() == 3);
    CHECK(model.geometry.vertices[0].normal == Vector3{.x = 0.0F, .y = 0.0F, .z = 1.0F});
}

TEST_CASE("a NaN normal fails the import") {
    const float nan = std::numeric_limits<float>::quiet_NaN();

    CHECK_FALSE(ImportGltfModel(test::BuildGlb({.normalZ = nan})).has_value());
}

TEST_CASE("a skin needs a bind matrix for every joint") {
    const test::GlbTriangle shortBinds{.parentName = "root", .isSkinned = true, .bindMatrices = 1};
    const test::GlbTriangle fullBinds{.parentName = "root", .isSkinned = true, .bindMatrices = 2};

    const Result<GltfModel> partial = ImportGltfModel(test::BuildGlb(shortBinds), ArmSkeleton());

    REQUIRE_FALSE(partial.has_value());
    CHECK(partial.error().message.contains("bind matrices"));
    CHECK(ImportGltfModel(test::BuildGlb(fullBinds), ArmSkeleton()).has_value());
}

}
}
