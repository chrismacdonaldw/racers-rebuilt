#include "app/racer/animation_player.hpp"

#include <doctest/doctest.h>

#include <array>
#include <numbers>

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

}
}
