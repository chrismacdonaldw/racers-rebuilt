#include "assets/sky_file.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"
#include "token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Ints;
using test::Join;

std::vector<std::byte> Concat(const std::vector<std::vector<std::byte>>& parts) {
    std::vector<std::byte> bytes;
    for (const std::vector<std::byte>& part : parts) {
        bytes.insert(bytes.end(), part.begin(), part.end());
    }
    return bytes;
}

std::vector<std::byte> Keyframe(const std::vector<std::byte>& fields) {
    return Join({Bytes({0x27}), test::Block(fields)});
}

std::vector<std::byte> Sky(std::string_view name, const std::vector<std::vector<std::byte>>& keys) {
    return Join({Bytes({0x27, 0x07, 0x0B, static_cast<std::uint8_t>(keys.size()), 0x08}),
                 test::Text(name), test::Block(Concat(keys))});
}

std::vector<std::byte> Skies(const std::vector<std::vector<std::byte>>& skies) {
    return Join({Bytes({0x2C}), test::ListHeader(static_cast<std::uint8_t>(skies.size())),
                 Concat(skies), Bytes({0x06})});
}

std::vector<std::byte> Colors() {
    return Join({Bytes({0x28}), Ints({100}), Bytes({0x29}), Ints({250, 250, 120}), Bytes({0x2A}),
                 Ints({100, 100, 255}), Bytes({0x2B}), Ints({25, 25, 255})});
}

TEST_CASE("a sky file reads its skies, initial sky and height offset") {
    const std::vector<std::byte> bytes =
        Join({Skies({Sky("daylight", {Keyframe(Colors())}), Sky("castle", {Keyframe({})})}),
              Bytes({0x2D}), test::Text("CASTLE"), Bytes({0x2E}), test::Floats({-30})});

    const Result<SkyFile> file = ParseSkyFile(bytes);

    REQUIRE(file.has_value());
    REQUIRE(file->skies.size() == 2);
    const SkyGradient& daylight = file->skies[0];
    CHECK(daylight.name == "daylight");
    REQUIRE(daylight.keyframes.size() == 1);
    CHECK(daylight.keyframes[0].length == Milliseconds{100});
    CHECK(daylight.keyframes[0].horizon == Rgb8{250, 250, 120});
    CHECK(daylight.keyframes[0].middle == Rgb8{100, 100, 255});
    CHECK(daylight.keyframes[0].top == Rgb8{25, 25, 255});
    CHECK(file->initial == 1);
    CHECK(file->heightOffset == -30.0F);
}

TEST_CASE("a keyframe without fields is white for a second") {
    const Result<SkyFile> file = ParseSkyFile(Skies({Sky("plain", {Keyframe({})})}));

    REQUIRE(file.has_value());
    const SkyKeyframe& keyframe = file->skies[0].keyframes[0];
    CHECK(keyframe.length == Milliseconds{1000});
    CHECK(keyframe.horizon == kWhiteRgb);
    CHECK(keyframe.top == kWhiteRgb);
    CHECK(file->initial == 0);
    CHECK(file->heightOffset == 0.0F);
}

TEST_CASE("an unknown initial sky starts on the first") {
    const std::vector<std::byte> bytes =
        Join({Skies({Sky("a", {Keyframe({})}), Sky("b", {Keyframe({})})}), Bytes({0x2D}),
              test::Text("missing")});

    const Result<SkyFile> file = ParseSkyFile(bytes);

    REQUIRE(file.has_value());
    CHECK(file->initial == 0);
}

TEST_CASE("a sky file without skies or keyframes fails") {
    CHECK_FALSE(ParseSkyFile(Skies({})).has_value());
    CHECK_FALSE(ParseSkyFile(Skies({Sky("empty", {})})).has_value());
}

TEST_CASE("a colour keeps the low byte of each channel") {
    const std::vector<std::byte> colour =
        Bytes({0x29, 0x0E, 0x00, 0x01, 0x0E, 0xFF, 0x01, 0x0D, 0xFF, 0xFF});

    const Result<SkyFile> file = ParseSkyFile(Skies({Sky("odd", {Keyframe(colour)})}));

    REQUIRE(file.has_value());
    CHECK(file->skies[0].keyframes[0].horizon == Rgb8{0, 255, 255});
}

TEST_CASE("sky names match on their first 8 characters, ignoring case") {
    CHECK(IsSameSkyName("Daylight", "DAYLIGHTS"));
    CHECK_FALSE(IsSameSkyName("dusk", "dawn"));
    CHECK_FALSE(IsSameSkyName("night", "nightfal"));
}

TEST_CASE("a keyframe with an unknown field fails") {
    CHECK_FALSE(ParseSkyFile(Skies({Sky("odd", {Keyframe(Join({Bytes({0x2C}), Ints({1})}))})}))
                    .has_value());
}

TEST_CASE("every sky file in the retail game parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    std::size_t skyCount = 0;
    std::size_t keyframeCount = 0;
    const test::Survey skies = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "SKB"; },
        [&](std::span<const std::byte> bytes) {
            return ParseSkyFile(bytes).transform([&](const SkyFile& file) {
                skyCount += file.skies.size();
                for (const SkyGradient& sky : file.skies) {
                    keyframeCount += sky.keyframes.size();
                }
            });
        });
    test::CheckSurvey(skies, 14);
    CHECK(skyCount == 16);
    CHECK(keyframeCount == 16);
}

}
}
