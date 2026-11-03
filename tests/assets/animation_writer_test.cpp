#include "assets/animation_writer.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"
#include "assets/animation_database.hpp"
#include "assets/jam_archive.hpp"

namespace rr::assets {
namespace {

constexpr std::size_t kRetailAnimations = 421;
constexpr float kRotationTolerance = 1e-6F;

bool IsNear(const Quaternion& a, const Quaternion& b) {
    return std::abs(a.w - b.w) <= kRotationTolerance && std::abs(a.x - b.x) <= kRotationTolerance &&
           std::abs(a.y - b.y) <= kRotationTolerance && std::abs(a.z - b.z) <= kRotationTolerance;
}

bool IsSame(const AnimationPart& a, const AnimationPart& b) {
    return a.name == b.name && a.firstTrack == b.firstTrack && a.frameCount == b.frameCount &&
           a.loopFrames == b.loopFrames && a.framesPerSecond == b.framesPerSecond &&
           a.velocity == b.velocity && a.bounds.centre == b.bounds.centre &&
           a.bounds.radius == b.bounds.radius;
}

bool IsSame(const AnimationAsset& a, const AnimationAsset& b) {
    return a.positionKeys == b.positionKeys && a.keyFrames == b.keyFrames && a.tracks == b.tracks &&
           std::ranges::equal(
               a.rotationKeys, b.rotationKeys,
               [](const Quaternion& x, const Quaternion& y) { return IsNear(x, y); }) &&
           std::ranges::equal(a.parts, b.parts, [](const AnimationPart& x, const AnimationPart& y) {
               return IsSame(x, y);
           });
}

Result<AnimationAsset> RoundTrip(const AnimationAsset& animations) {
    return WriteAnimations(animations).and_then([](const std::vector<std::byte>& bytes) {
        return ParseAnimations(bytes);
    });
}

AnimationAsset Wave() {
    const Quaternion turned = FromAxisAngle({.x = 0.0F, .y = 0.0F, .z = 1.0F}, 1.0F);
    return {.positionKeys = {{.x = 1.0F, .y = 2.0F, .z = 3.0F}, {.x = 4.0F, .y = 5.0F, .z = 6.0F}},
            .rotationKeys = {Quaternion{}, turned},
            .keyFrames = {0, 10, 20},
            .tracks = {{.rotations = {.firstValue = 0, .firstFrame = 0, .count = 2},
                        .positions = {.firstValue = 0, .firstFrame = 1, .count = 2}},
                       {.rotations = {.firstValue = 1, .count = 1}}},
            .parts = {{.name = "wave",
                       .frameCount = 20,
                       .loopFrames = 21,
                       .framesPerSecond = 30,
                       .velocity = {.x = 0.5F},
                       .bounds = {.centre = {.y = 1.0F}, .radius = 2.0F}}}};
}

TEST_CASE("written animations read back with the same keys, tracks and parts") {
    const AnimationAsset wave = Wave();

    const Result<AnimationAsset> back = RoundTrip(wave);

    REQUIRE(back.has_value());
    CHECK(IsSame(*back, wave));
}

TEST_CASE("animations with empty key pools still read back") {
    const AnimationAsset still{
        .tracks = {{}}, .parts = {{.name = "still", .loopFrames = 1, .framesPerSecond = 30}}};

    const Result<AnimationAsset> back = RoundTrip(still);

    REQUIRE(back.has_value());
    CHECK(back->tracks == still.tracks);
    CHECK(back->parts.at(0).name == "still");
}

TEST_CASE("animations without tracks or parts can't be written") {
    AnimationAsset noParts = Wave();
    noParts.parts.clear();
    AnimationAsset noTracks = Wave();
    noTracks.tracks.clear();

    CHECK_FALSE(WriteAnimations(noParts).has_value());
    CHECK_FALSE(WriteAnimations(noTracks).has_value());
}

TEST_CASE("every retail animation file writes and reads back the same") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    const test::Survey animations = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "ADB"; },
        [](std::span<const std::byte> bytes) {
            return ParseAnimations(bytes).and_then(
                [](const AnimationAsset& original) -> Result<void> {
                    const Result<AnimationAsset> back = RoundTrip(original);
                    if (!back) {
                        return std::unexpected(back.error());
                    }
                    if (!IsSame(*back, original)) {
                        return Fail(ErrorCode::InvalidData, "the animations changed");
                    }
                    return {};
                });
        });

    test::CheckSurvey(animations, kRetailAnimations);
}

}
}
