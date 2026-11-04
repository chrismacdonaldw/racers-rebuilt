#include "assets/gltf_export.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/glb_writer.hpp"
#include "assets/gltf_animation.hpp"
#include "assets/gltf_model.hpp"
#include "core/quaternion.hpp"

namespace rr::assets {
namespace {

constexpr float kScale = 0.5F;
constexpr std::int32_t kRate = 30;
constexpr float kQuarterTurn = std::numbers::pi_v<float> / 2.0F;

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x).epsilon(1e-5));
    CHECK(actual.y == doctest::Approx(expected.y).epsilon(1e-5));
    CHECK(actual.z == doctest::Approx(expected.z).epsilon(1e-5));
}

void CheckVertex(const GeometryVertex& actual, const GeometryVertex& expected) {
    CheckNear(actual.position, expected.position);
    CheckNear(actual.normal, expected.normal);
    CHECK(actual.joint == expected.joint);
}

SkeletonAsset Arm() {
    return {.nodes = {{.name = "root"},
                      {.name = "arm",
                       .position = {.x = 2.0F},
                       .rotation = FromAxisAngle({.z = 1.0F}, kQuarterTurn),
                       .parent = 0}}};
}

GeometryAsset Elbow() {
    return {
        .vertices =
            {{.position = {.x = 1.0F}, .normal = {.z = 1.0F}, .u = 0.25F, .v = 0.75F, .joint = 1},
             {.position = {.y = 1.0F}, .normal = {.x = 1.0F}, .joint = 1},
             {.position = {.z = 1.0F}, .normal = {.y = 1.0F}},
             {.position = {.x = 2.0F}, .normal = {.z = -1.0F}},
             {.position = {.y = 2.0F}, .normal = {.y = -1.0F}, .joint = 1},
             {.position = {.z = 2.0F}, .normal = {.x = -1.0F}}},
        .indices = {0, 1, 2, 3, 4, 5, 2, 1, 0},
        .materials = {"SKIN", "SHIRT"},
        .ranges = {{.firstIndex = 0, .indexCount = 3, .material = 0},
                   {.firstIndex = 3, .indexCount = 3, .material = 1},
                   {.firstIndex = 6, .indexCount = 3, .material = 0}},
        .scale = kScale};
}

AnimationAsset Wave() {
    return {.positionKeys = {{.x = 4.0F}},
            .rotationKeys = {Quaternion{}, FromAxisAngle({.z = 1.0F}, kQuarterTurn)},
            .keyFrames = {0, 15},
            .tracks = {{.positions = {.count = 1}}, {.rotations = {.count = 2}}},
            .parts = {{.name = "WAVE",
                       .firstTrack = 0,
                       .frameCount = 15,
                       .loopFrames = 16,
                       .framesPerSecond = kRate}}};
}

std::vector<std::byte> Export(const GltfSource& source) {
    Result<std::vector<std::byte>> glb = ExportGltf(source);
    REQUIRE(glb.has_value());
    return std::move(*glb);
}

GltfModel Import(const std::vector<std::byte>& glb, const GltfTarget& target) {
    Result<GltfModel> model = ImportGltfModel(glb, target);
    REQUIRE(model.has_value());
    return std::move(*model);
}

GltfModel ImportElbow() {
    const GeometryAsset elbow = Elbow();
    const SkeletonAsset arm = Arm();
    return Import(Export({.geometry = &elbow, .skeleton = &arm}),
                  {.joints = {"ROOT", "ARM"}, .scale = kScale});
}

TEST_CASE("an exported skinned model imports back with its materials and scale") {
    const GltfModel model = ImportElbow();

    CHECK(model.vertices == GdbVertices::Lit);
    CHECK(model.geometry.scale == kScale);
    CHECK(model.geometry.materials == Elbow().materials);
    CHECK(model.geometry.vertices[0].u == 0.25F);
    CHECK(model.geometry.vertices[0].v == 0.75F);
}

TEST_CASE("an exported skinned model imports back with each vertex on its joint") {
    const GltfModel model = ImportElbow();
    const GeometryAsset elbow = Elbow();

    REQUIRE(model.geometry.vertices.size() == elbow.vertices.size());
    for (std::size_t i = 0; i < elbow.vertices.size(); ++i) {
        CheckVertex(model.geometry.vertices[i], elbow.vertices[i]);
    }
}

TEST_CASE("an exported model has one primitive for each material it uses") {
    const GeometryAsset elbow = Elbow();

    const GltfModel model = Import(Export({.geometry = &elbow}), {});

    CHECK(model.geometry.indices == std::vector<std::uint32_t>{0, 1, 2, 2, 1, 0, 3, 4, 5});
    REQUIRE(model.geometry.ranges.size() == 2);
    CHECK(model.geometry.ranges[0].indexCount == 6);
    CHECK(model.geometry.ranges[1].material == 1);
}

TEST_CASE("a model without normals exports its vertex colours") {
    const GeometryAsset patch{.vertices = {{.rgba = {200, 100, 50, 128}},
                                           {.position = {.x = 1.0F}},
                                           {.position = {.y = 1.0F}}},
                              .indices = {0, 1, 2},
                              .materials = {"GRASS"},
                              .ranges = {{.indexCount = 3}}};

    const GltfModel model = Import(Export({.geometry = &patch}), {});

    CHECK(model.vertices == GdbVertices::Colored);
    CHECK(model.geometry.vertices[0].rgba == Rgba8{200, 100, 50, 128});
    CHECK(model.geometry.vertices[1].rgba == kOpaqueWhite);
}

TEST_CASE("each animation part exports as a glTF animation of the same name") {
    const GeometryAsset elbow = Elbow();
    const SkeletonAsset arm = Arm();
    const AnimationAsset wave = Wave();
    const std::vector<std::byte> glb =
        Export({.geometry = &elbow, .skeleton = &arm, .animations = &wave});

    AnimationAsset original = wave;
    original.rotationKeys = {Quaternion{}};
    original.tracks = {{}, {.rotations = {.count = 1}}};
    const Result<AnimationAsset> back = ImportGltfAnimations(
        glb, {.joints = {"ROOT", "ARM"}, .scale = kScale, .original = std::move(original)});

    REQUIRE(back.has_value());
    REQUIRE(back->parts.size() == 1);
    CHECK(back->parts[0].frameCount == 15);
    CHECK(back->parts[0].loopFrames == 16);
    const AnimationTrack& turn = back->tracks.at(1);
    REQUIRE(turn.rotations.count == 2);
    CHECK(back->keyFrames.at(turn.rotations.firstFrame + 1) == 15);
    const Quaternion& end = back->rotationKeys.at(turn.rotations.firstValue + 1);
    const Quaternion expected = FromAxisAngle({.z = 1.0F}, kQuarterTurn);
    CHECK(end.w == doctest::Approx(expected.w));
    CHECK(end.z == doctest::Approx(expected.z));
    const AnimationTrack& root = back->tracks.at(0);
    REQUIRE(root.positions.count == 1);
    CheckNear(back->positionKeys.at(root.positions.firstValue), {.x = 4.0F});
}

TEST_CASE("animations with fewer tracks than the skeleton has nodes aren't exported") {
    const GeometryAsset elbow = Elbow();
    const SkeletonAsset arm = Arm();
    AnimationAsset wave = Wave();
    wave.tracks.pop_back();

    CHECK_FALSE(
        ExportGltf({.geometry = &elbow, .skeleton = &arm, .animations = &wave}).has_value());
}

TEST_CASE("a vertex on a joint the skeleton doesn't have can't be exported") {
    GeometryAsset elbow = Elbow();
    elbow.vertices[0].joint = 2;
    const SkeletonAsset arm = Arm();

    CHECK_FALSE(ExportGltf({.geometry = &elbow, .skeleton = &arm}).has_value());
}

TEST_CASE("a model without vertices can't be exported") {
    const GeometryAsset empty;

    CHECK_FALSE(ExportGltf({.geometry = &empty}).has_value());
}

TEST_CASE("JSON strings escape quotes, backslashes and control characters") {
    constexpr std::string_view kEscaped = R"("a\"b\\c\u000a")";

    CHECK(JsonString("a\"b\\c\n") == kEscaped);
}

}
}
