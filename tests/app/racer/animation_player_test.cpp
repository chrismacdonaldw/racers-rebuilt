#include "app/racer/animation_player.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace rr::app {
namespace {

constexpr float kQuarterTurn = std::numbers::pi_v<float> / 2.0F;

assets::AnimationAsset Slide() {
    return {
        .positionKeys = {{.x = 0.0F}, {.x = 10.0F}, {.z = 4.0F}},
        .rotationKeys = {Quaternion{}, FromAxisAngle({.z = 1.0F}, kQuarterTurn)},
        .keyFrames = {0, 10, 20},
        .tracks = {{.rotations = {.firstValue = 0, .firstFrame = 1, .count = 2},
                    .positions = {.firstValue = 0, .firstFrame = 0, .count = 2}},
                   {},
                   {.positions = {.firstValue = 2, .count = 1}},
                   {}},
        .parts = {{.name = "slide",
                   .firstTrack = 0,
                   .frameCount = 21,
                   .loopFrames = 22,
                   .framesPerSecond = 30},
                  {.name = "lift",
                   .firstTrack = 2,
                   .frameCount = 1,
                   .loopFrames = 2,
                   .framesPerSecond = 30}},
    };
}

AnimationPlayer MakePlayer(const assets::AnimationAsset& animations) {
    Result<AnimationPlayer> player = AnimationPlayer::Create(animations, 2);
    REQUIRE(player.has_value());
    return std::move(*player);
}

std::array<Transform, 2> Posed(AnimationPlayer& player) {
    std::array<Transform, 2> locals{Transform{.position = {.y = 7.0F}}, Transform{}};
    player.PoseNodes(locals);
    return locals;
}

TEST_CASE("a part interpolates between the keys around its frame") {
    const assets::AnimationAsset animations = Slide();
    AnimationPlayer player = MakePlayer(animations);
    player.Play(0, true);

    player.AdvanceFrames(5.0F);
    const std::array<Transform, 2> posed = Posed(player);

    CHECK(posed[0].position == Vector3{.x = 5.0F});
    CHECK(posed[1].position == Vector3{});
}

TEST_CASE("a looping part runs from its last key back to its first") {
    const assets::AnimationAsset animations = Slide();
    AnimationPlayer player = MakePlayer(animations);
    player.Play(0, true);

    player.AdvanceFrames(16.0F);
    const std::array<Transform, 2> wrapping = Posed(player);
    player.AdvanceFrames(10.0F);
    const std::array<Transform, 2> wrapped = Posed(player);

    CHECK(wrapping[0].position.x == doctest::Approx(5.0F));
    CHECK(wrapped[0].position.x == doctest::Approx(4.0F));
}

TEST_CASE("a one-shot part holds its last frame and wraps over its frame count") {
    const assets::AnimationAsset animations = Slide();
    AnimationPlayer player = MakePlayer(animations);
    player.Play(0, false);

    player.AdvanceFrames(100.0F);
    const std::array<Transform, 2> posed = Posed(player);

    CHECK(posed[0].position.x == doctest::Approx(10.0F / 11.0F));
    CHECK(posed[0].rotation.z == doctest::Approx(std::sin(kQuarterTurn / 2.0F)));
}

TEST_CASE("a node without keys keeps its placement") {
    const assets::AnimationAsset animations = Slide();
    AnimationPlayer player = MakePlayer(animations);
    player.Play(1, true);

    const std::array<Transform, 2> posed = Posed(player);

    CHECK(posed[0].position == Vector3{.z = 4.0F});
    CHECK(posed[1].position == Vector3{});
}

TEST_CASE("clocks run at the part's frame rate") {
    const assets::AnimationAsset animations = Slide();
    AnimationPlayer player = MakePlayer(animations);
    player.Play(0, true);

    player.Advance(100.0F);

    CHECK(Posed(player)[0].position.x == doctest::Approx(3.0F));
}

TEST_CASE("a blend moves from the current part to the next over its time") {
    const assets::AnimationAsset animations = Slide();
    AnimationPlayer player = MakePlayer(animations);
    player.Play(1, true);
    player.BlendTo(0, 300.0F);

    player.Advance(150.0F);
    const std::array<Transform, 2> halfway = Posed(player);
    player.Advance(150.0F);
    const std::array<Transform, 2> finished = Posed(player);

    CHECK(halfway[0].position.x == doctest::Approx(2.25F));
    CHECK(halfway[0].position.z == doctest::Approx(2.0F));
    CHECK(finished[0].position.x == doctest::Approx(9.0F));
    CHECK(finished[0].position.z == doctest::Approx(0.0F));
}

TEST_CASE("playing the current part again keeps its clock") {
    const assets::AnimationAsset animations = Slide();
    AnimationPlayer player = MakePlayer(animations);
    player.Play(0, true);
    player.AdvanceFrames(5.0F);

    player.Play(0, true);

    CHECK(Posed(player)[0].position.x == doctest::Approx(5.0F));
}

TEST_CASE("parts are found by name") {
    const assets::AnimationAsset animations = Slide();
    const AnimationPlayer player = MakePlayer(animations);

    CHECK(player.FindPart("lift") == 1);
    CHECK_FALSE(player.FindPart("steer-l").has_value());
}

TEST_CASE("animations with fewer tracks than the skeleton needs fail") {
    const assets::AnimationAsset animations = Slide();

    CHECK_FALSE(AnimationPlayer::Create(animations, 3).has_value());
}

}
}
