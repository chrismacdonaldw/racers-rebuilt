#include "assets/gltf_animation.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "assets/animation_database.hpp"
#include "gltf_files.hpp"

namespace rr::assets {
namespace {

constexpr float kDriverScale = 1.0F / 64.0F;
constexpr std::int32_t kRate = 30;

AnimationAsset IdleAndWalk() {
    AnimationAsset original{.positionKeys = {{.x = 7.0F}},
                            .rotationKeys = {Quaternion{}},
                            .tracks = {{.positions = {.count = 1}},
                                       {.rotations = {.count = 1}},
                                       {.positions = {.count = 1}},
                                       {.rotations = {.count = 1}}}};
    original.parts = {{.name = "IDLE", .firstTrack = 0, .loopFrames = 1, .framesPerSecond = kRate},
                      {.name = "WALK",
                       .firstTrack = 2,
                       .frameCount = 4,
                       .loopFrames = 5,
                       .framesPerSecond = kRate,
                       .bounds = {.radius = 3.0F}}};
    return original;
}

GltfAnimationTarget ArmTarget() {
    return {.joints = {"ROOT", "ARM"}, .scale = kDriverScale, .original = IdleAndWalk()};
}

AnimationAsset Import(const test::GlbTriangle& triangle) {
    Result<AnimationAsset> animations = ImportGltfAnimations(test::BuildGlb(triangle), ArmTarget());
    REQUIRE(animations.has_value());
    return std::move(*animations);
}

TEST_CASE("a glTF animation replaces the game part with its name and keeps the part's bounds") {
    const AnimationAsset animations = Import({.parentName = "root", .animation = "Walk"});

    REQUIRE(animations.parts.size() == 2);
    const AnimationPart& walk = animations.parts[1];
    CHECK(walk.firstTrack == 2);
    CHECK(walk.frameCount == kRate);
    CHECK(walk.loopFrames == kRate + 1);
    CHECK(walk.bounds.radius == 3.0F);
    REQUIRE(animations.tracks.size() == 4);
}

TEST_CASE("joints the glTF animation doesn't move keep the game part's keys") {
    const AnimationAsset animations = Import({.parentName = "root", .animation = "walk"});

    const AnimationTrack& root = animations.tracks.at(2);
    CHECK(root.rotations.count == 0);
    REQUIRE(root.positions.count == 1);
    CHECK(animations.positionKeys.at(root.positions.firstValue) == Vector3{.x = 7.0F});
}

TEST_CASE("a glTF animation that moves no skeleton joint keeps the part's length") {
    GltfAnimationTarget target = ArmTarget();
    target.joints = {"ROOT", "LEG"};

    const Result<AnimationAsset> animations =
        ImportGltfAnimations(test::BuildGlb({.parentName = "root", .animation = "walk"}), target);

    REQUIRE(animations.has_value());
    CHECK(animations->parts.at(1).frameCount == 4);
    CHECK(animations->parts.at(1).loopFrames == 5);
    CHECK(animations->tracks.at(3).rotations.count == 1);
}

TEST_CASE("a glTF rotation is turned into game axes at the part's frame rate") {
    const AnimationAsset animations = Import({.parentName = "root", .animation = "walk"});

    const KeyRun& turns = animations.tracks.at(3).rotations;
    REQUIRE(turns.count == 2);
    CHECK(animations.keyFrames.at(turns.firstFrame + 1) == kRate);
    const Quaternion& turned = animations.rotationKeys.at(turns.firstValue + 1);
    CHECK(turned.w == doctest::Approx(0.70710677F));
    CHECK(turned.x == doctest::Approx(0.0F));
    CHECK(turned.y == doctest::Approx(0.0F));
    CHECK(turned.z == doctest::Approx(0.70710677F));
}

TEST_CASE("a stepped glTF move holds each key until the next, in model units") {
    const AnimationAsset animations = Import({.parentName = "root", .animation = "walk"});

    const KeyRun& moves = animations.tracks.at(3).positions;
    REQUIRE(moves.count == 3);
    const std::vector<std::uint16_t> frames(
        animations.keyFrames.begin() + moves.firstFrame,
        animations.keyFrames.begin() + moves.firstFrame + moves.count);
    CHECK(frames == std::vector<std::uint16_t>{0, kRate / 2, kRate / 2});
    CHECK(animations.positionKeys.at(moves.firstValue + 1) == Vector3{});
    CHECK(animations.positionKeys.at(moves.firstValue + 2) == Vector3{.x = 64.0F});
}

TEST_CASE("game parts without a glTF animation keep their keys") {
    const AnimationAsset animations = Import({.parentName = "root", .animation = "walk"});

    const AnimationPart& idle = animations.parts.at(0);
    CHECK(idle.firstTrack == 0);
    CHECK(idle.loopFrames == 1);
    const KeyRun& kept = animations.tracks.at(0).positions;
    REQUIRE(kept.count == 1);
    CHECK(animations.positionKeys.at(kept.firstValue) == Vector3{.x = 7.0F});
    CHECK(animations.tracks.at(1).rotations.count == 1);
}

TEST_CASE("a glTF file without animations isn't an animation mod") {
    const Result<AnimationAsset> animations =
        ImportGltfAnimations(test::BuildGlb({.parentName = "root"}), ArmTarget());

    REQUIRE_FALSE(animations.has_value());
    CHECK(animations.error().code == ErrorCode::NotFound);
}

TEST_CASE("cubic spline glTF animations are refused") {
    const test::GlbTriangle cubic{
        .parentName = "root", .animation = "walk", .turnInterpolation = "CUBICSPLINE"};

    CHECK_FALSE(ImportGltfAnimations(test::BuildGlb(cubic), ArmTarget()).has_value());
}

TEST_CASE("game parts with fewer tracks than the skeleton has nodes are refused") {
    GltfAnimationTarget target = ArmTarget();
    target.original.tracks.pop_back();

    CHECK_FALSE(
        ImportGltfAnimations(test::BuildGlb({.parentName = "root", .animation = "run"}), target)
            .has_value());
}

}
}
