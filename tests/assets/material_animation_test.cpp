#include "assets/material_animation.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "archive_survey.hpp"
#include "assets/jam_archive.hpp"
#include "token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Ints;
using test::Join;

std::vector<std::byte> Keys(std::initializer_list<std::pair<std::string_view, std::uint8_t>> keys) {
    std::vector<std::byte> bytes =
        Join({Bytes({0x27}), test::ListHeader(static_cast<std::uint8_t>(keys.size()))});
    for (const auto& [material, frame] : keys) {
        const std::vector<std::byte> key = Join({test::Text(material), Ints({frame})});
        bytes.insert(bytes.end(), key.begin(), key.end());
    }
    bytes.push_back(std::byte{0x06});
    return bytes;
}

std::vector<std::byte> Track(std::uint8_t first, std::uint8_t count, std::uint8_t duration,
                             std::uint8_t rate = 30) {
    return Join({Bytes({0x28, 0x05, 0x27}), Ints({first, count}), Bytes({0x29}), Ints({duration}),
                 Bytes({0x2A}), Ints({rate}), Bytes({0x06})});
}

std::vector<std::byte> Tracks(std::initializer_list<std::vector<std::byte>> tracks) {
    return Join({Bytes({0x28}), test::ListHeader(static_cast<std::uint8_t>(tracks.size())),
                 Join(tracks), Bytes({0x06})});
}

std::vector<std::byte> SmokeKeys() {
    return Keys({{"smoke1", 0}, {"smoke2", 8}, {"smoke3", 16}, {"smoke4", 24}, {"dust1", 5}});
}

TEST_CASE("a material animation reads its keys and tracks") {
    const Result<MaterialAnimationAsset> animation =
        ParseMaterialAnimation(Join({SmokeKeys(), Tracks({Track(0, 4, 32), Track(4, 1, 6, 60)})}));

    REQUIRE(animation.has_value());
    REQUIRE(animation->keys.size() == 5);
    CHECK(animation->keys[1] == MaterialKey{.material = "smoke2", .frame = 8});
    CHECK(animation->tracks ==
          std::vector<MaterialTrack>{
              {.firstKey = 0, .keyCount = 4, .duration = 32},
              {.firstKey = 4, .keyCount = 1, .duration = 6, .framesPerSecond = 60}});
}

TEST_CASE("a track is sampled by age and restarts each cycle") {
    const Result<MaterialAnimationAsset> animation =
        ParseMaterialAnimation(Join({SmokeKeys(), Tracks({Track(0, 4, 32)})}));
    REQUIRE(animation.has_value());

    CHECK(SampleTrack(*animation, 0, Milliseconds{0}) == 0);
    CHECK(SampleTrack(*animation, 0, Milliseconds{266}) == 0);
    CHECK(SampleTrack(*animation, 0, Milliseconds{267}) == 1);
    CHECK(SampleTrack(*animation, 0, Milliseconds{1000}) == 3);
    CHECK(SampleTrack(*animation, 0, Milliseconds{1067}) == 0);
}

TEST_CASE("a track shows its last key before its first key") {
    const Result<MaterialAnimationAsset> animation = ParseMaterialAnimation(
        Join({Keys({{"blink", 5}, {"happy", 10}}), Tracks({Track(0, 2, 20)})}));
    REQUIRE(animation.has_value());

    CHECK(SampleTrack(*animation, 0, Milliseconds{0}) == 1);
    CHECK(SampleTrack(*animation, 0, Milliseconds{200}) == 0);
    CHECK(SampleTrack(*animation, 0, Milliseconds{400}) == 1);
}

TEST_CASE("a material animation rejects tracks that don't fit their keys") {
    CHECK_FALSE(ParseMaterialAnimation(Join({Keys({}), Tracks({Track(0, 1, 1)})})).has_value());
    CHECK_FALSE(ParseMaterialAnimation(Join({SmokeKeys(), Tracks({})})).has_value());
    CHECK_FALSE(ParseMaterialAnimation(Join({SmokeKeys(), Tracks({Track(3, 3, 32)})})).has_value());
    CHECK_FALSE(ParseMaterialAnimation(Join({SmokeKeys(), Tracks({Track(2, 3, 32)})})).has_value());
    CHECK_FALSE(ParseMaterialAnimation(Join({SmokeKeys(), Tracks({Track(0, 4, 24)})})).has_value());
    CHECK_FALSE(
        ParseMaterialAnimation(Join({SmokeKeys(), Bytes({0x28}), test::ListHeader(1),
                                     Bytes({0x28, 0x05, 0x2B}), Ints({1}), Bytes({0x06, 0x06})}))
            .has_value());
}

TEST_CASE("every material animation in the retail game parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    std::size_t keys = 0;
    std::size_t tracks = 0;
    const test::Survey files = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "MAB"; },
        [&](std::span<const std::byte> bytes) {
            return ParseMaterialAnimation(bytes).transform(
                [&](const MaterialAnimationAsset& parsed) {
                    keys += parsed.keys.size();
                    tracks += parsed.tracks.size();
                });
        });
    test::CheckSurvey(files, 42);
    CHECK(keys == 4877);
    CHECK(tracks == 713);
}

}
}
