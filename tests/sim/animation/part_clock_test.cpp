#include "sim/animation/part_clock.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <numbers>

namespace rr::sim {
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

std::array<Transform, 2> Posed(const assets::AnimationAsset& animations, const PartClock& clock) {
    std::array<Transform, 2> locals{Transform{.position = {.y = 7.0F}}, Transform{}};
    SamplePart(animations, clock, locals);
    return locals;
}

TEST_CASE("a part interpolates between the keys around its frame") {
    const std::array<Transform, 2> posed = Posed(Slide(), {.frame = 5.0F});

    CHECK(posed[0].position == Vector3{.x = 5.0F});
    CHECK(posed[1].position == Vector3{});
}

TEST_CASE("a looping part runs from its last key back to its first") {
    const assets::AnimationAsset animations = Slide();
    PartClock clock;

    AdvanceFrames(animations, clock, 16.0F);
    const std::array<Transform, 2> wrapping = Posed(animations, clock);
    AdvanceFrames(animations, clock, 10.0F);
    const std::array<Transform, 2> wrapped = Posed(animations, clock);

    CHECK(wrapping[0].position.x == doctest::Approx(5.0F));
    CHECK(clock.frame == doctest::Approx(4.0F));
    CHECK(wrapped[0].position.x == doctest::Approx(4.0F));
}

TEST_CASE("a one-shot part holds its last frame and wraps over its frame count") {
    const assets::AnimationAsset animations = Slide();
    PartClock clock{.loops = false};

    AdvanceFrames(animations, clock, 100.0F);
    const std::array<Transform, 2> posed = Posed(animations, clock);

    CHECK(IsHoldingLastFrame(animations, clock));
    CHECK(posed[0].position.x == doctest::Approx(10.0F / 11.0F));
    CHECK(posed[0].rotation.z == doctest::Approx(std::sin(kQuarterTurn / 2.0F)));
}

TEST_CASE("keys that share a frame jump to the later key") {
    const assets::AnimationAsset animations{
        .positionKeys = {{.x = 0.0F}, {.x = 10.0F}, {.x = 30.0F}, {.x = 40.0F}},
        .rotationKeys = {},
        .keyFrames = {0, 10, 10, 20},
        .tracks = {{.rotations = {}, .positions = {.firstValue = 0, .firstFrame = 0, .count = 4}},
                   {}},
        .parts = {{.name = "jump",
                   .firstTrack = 0,
                   .frameCount = 22,
                   .loopFrames = 20,
                   .framesPerSecond = 30}},
    };

    CHECK(Posed(animations, {.frame = 10.0F})[0].position.x == doctest::Approx(30.0F));
    CHECK(Posed(animations, {.frame = 20.5F})[0].position.x == doctest::Approx(40.0F));
}

TEST_CASE("a node without keys keeps its placement") {
    const std::array<Transform, 2> posed = Posed(Slide(), {.part = 1});

    CHECK(posed[0].position == Vector3{.z = 4.0F});
    CHECK(posed[1].position == Vector3{});
}

TEST_CASE("clocks run at the part's frame rate") {
    const assets::AnimationAsset animations = Slide();
    PartClock clock;

    AdvanceClock(animations, clock, 100.0F);

    CHECK(clock.frame == doctest::Approx(3.0F));
}

TEST_CASE("rewinding a looping part wraps over its loop length, and a one-shot part holds") {
    const assets::AnimationAsset animations = Slide();
    PartClock looping{.frame = 1.0F};
    PartClock once{.frame = 1.0F, .loops = false};

    RewindClock(animations, looping, 100.0F);
    RewindClock(animations, once, 100.0F);

    CHECK(looping.frame == doctest::Approx(20.0F));
    CHECK(once.frame == 0.0F);
}

TEST_CASE("animations with fewer tracks than the skeleton needs fail") {
    CHECK(CheckPartTracks(Slide(), 2).has_value());
    CHECK_FALSE(CheckPartTracks(Slide(), 3).has_value());
}

TEST_CASE("a clock interpolates within a part and jumps over a wrap") {
    const PartClock from{.frame = 4.0F};

    CHECK(InterpolateClock(from, {.frame = 6.0F}, 0.25F) == PartClock{.frame = 4.5F});
    CHECK(InterpolateClock(from, {.frame = 1.0F}, 0.25F) == PartClock{.frame = 1.0F});
    CHECK(InterpolateClock(from, {.part = 1, .frame = 6.0F}, 0.25F) ==
          PartClock{.part = 1, .frame = 6.0F});
}

}
}
