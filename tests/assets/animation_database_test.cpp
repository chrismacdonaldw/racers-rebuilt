#include "assets/animation_database.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Block;
using test::Bytes;
using test::Floats;
using test::Ints;
using test::Join;
using test::ListHeader;
using test::NamedEntry;

std::vector<std::byte> List(std::uint8_t count, const std::vector<std::byte>& body) {
    return Join({ListHeader(count), body, Bytes({0x06})});
}

std::vector<std::byte> Pools(std::initializer_list<std::uint8_t> frames) {
    return Join({Bytes({0x27}),
                 Block(Join({Bytes({0x28}), List(2, Floats({1, 2, 3, 4, 5, 6})), Bytes({0x29}),
                             List(2, Floats({0, 0, 0, 1, 0, 0, 1, 1})), Bytes({0x2A}),
                             List(static_cast<std::uint8_t>(frames.size()), Ints(frames))}))});
}

std::vector<std::byte> Tracks(std::initializer_list<std::uint8_t> first,
                              std::initializer_list<std::uint8_t> second) {
    return Join({Bytes({0x2B}), List(2, Join({Ints(first), Ints(second)}))});
}

std::vector<std::byte> Parts(std::uint8_t firstTrack) {
    return Join({Bytes({0x2C}),
                 List(1, NamedEntry(0x2C, "frwrd",
                                    Join({Bytes({0x2B}), Ints({firstTrack}), Bytes({0x2D}),
                                          Ints({10}), Bytes({0x2E}), Ints({11}), Bytes({0x2F}),
                                          Ints({30}), Bytes({0x30}), Floats({0, 0, 0}),
                                          Bytes({0x31}), Floats({1, 2, 3, 5})})))});
}

TEST_CASE("animations read key pools, tracks and parts") {
    const std::vector adb =
        Join({Pools({0, 5, 10}), Tracks({0, 0, 2, 0, 1, 2}, {1, 0, 1, 0, 0, 0}), Parts(1)});

    const Result<AnimationAsset> animations = ParseAnimations(adb);

    REQUIRE(animations.has_value());
    CHECK(animations->positionKeys.at(1) == Vector3{.x = 4.0F, .y = 5.0F, .z = 6.0F});
    REQUIRE(animations->rotationKeys.size() == 2);
    CHECK(animations->rotationKeys[1].z == doctest::Approx(-0.70710677F));
    CHECK(animations->keyFrames == std::vector<std::uint16_t>{0, 5, 10});
    REQUIRE(animations->tracks.size() == 2);
    CHECK(animations->tracks[0] ==
          AnimationTrack{.rotations = {.firstValue = 0, .firstFrame = 0, .count = 2},
                         .positions = {.firstValue = 0, .firstFrame = 1, .count = 2}});
    CHECK(animations->tracks[1].rotations.count == 1);
    CHECK(animations->tracks[1].positions.count == 0);
    REQUIRE(animations->parts.size() == 1);
    const AnimationPart& part = animations->parts.front();
    CHECK(part.name == "frwrd");
    CHECK(part.firstTrack == 1);
    CHECK(part.frameCount == 10);
    CHECK(part.loopFrames == 11);
    CHECK(part.framesPerSecond == 30);
    CHECK(part.bounds.centre == Vector3{.x = 1.0F, .y = 2.0F, .z = 3.0F});
    CHECK(part.bounds.radius == 5.0F);
}

TEST_CASE("an animation track whose keys run past their pool fails") {
    CHECK_FALSE(ParseAnimations(Join({Pools({0, 5, 10}),
                                      Tracks({1, 0, 2, 0, 0, 0}, {0, 0, 0, 0, 0, 0}), Parts(0)}))
                    .has_value());
    CHECK_FALSE(ParseAnimations(Join({Pools({0, 5, 10}),
                                      Tracks({0, 2, 2, 0, 0, 0}, {0, 0, 0, 0, 0, 0}), Parts(0)}))
                    .has_value());
    CHECK_FALSE(ParseAnimations(Join({Pools({0, 5, 10}),
                                      Tracks({2, 0, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0}), Parts(0)}))
                    .has_value());
}

TEST_CASE("an animation track with key frames out of order fails") {
    CHECK_FALSE(ParseAnimations(Join({Pools({5, 0, 10}),
                                      Tracks({0, 0, 2, 0, 0, 0}, {0, 0, 0, 0, 0, 0}), Parts(0)}))
                    .has_value());
}

TEST_CASE("an animation part that starts past the tracks fails") {
    CHECK_FALSE(ParseAnimations(Join({Pools({0, 5, 10}),
                                      Tracks({0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}), Parts(2)}))
                    .has_value());
}

TEST_CASE("animations without parts fail") {
    CHECK_FALSE(
        ParseAnimations(Join({Pools({0, 5, 10}), Tracks({0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0})}))
            .has_value());
}

}
}
