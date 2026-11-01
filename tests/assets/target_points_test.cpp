#include "assets/target_points.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"
#include "assets/jam_archive.hpp"
#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Floats;
using test::Ints;
using test::Join;

std::vector<std::byte> Point(std::int16_t x, std::optional<std::uint8_t> event = std::nullopt) {
    const std::vector<std::byte> index =
        event ? Join({Bytes({0x29}), Ints({*event})}) : std::vector<std::byte>{};
    return Join({Bytes({0x27, 0x05, 0x28}), Floats({x, 2, 3}), index, Bytes({0x06})});
}

std::vector<std::byte> File(std::initializer_list<std::vector<std::byte>> points) {
    return Join({Bytes({0x27}), test::ListHeader(static_cast<std::uint8_t>(points.size())),
                 Join(points), Bytes({0x06})});
}

TEST_CASE("a target point file reads each position and the track event that removes it") {
    const Result<std::vector<TargetPoint>> points = ParseTgb(File({Point(1, 7), Point(4)}));

    REQUIRE(points.has_value());
    CHECK(*points == std::vector<TargetPoint>{{.position = {1, 2, 3}, .removingEvent = 7},
                                              {.position = {4, 2, 3}}});
}

TEST_CASE("a target point file rejects a point without a position or an unknown field") {
    CHECK_FALSE(ParseTgb(File({Bytes({0x27, 0x05, 0x06})})).has_value());
    CHECK_FALSE(
        ParseTgb(File({Join({Bytes({0x27, 0x05, 0x28}), Floats({0, 0, 0}), Bytes({0x2A, 0x06})})}))
            .has_value());
}

TEST_CASE("every target point file in the retail game parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    std::size_t points = 0;
    const test::Survey files = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "TGB"; },
        [&](std::span<const std::byte> bytes) {
            return ParseTgb(bytes).transform(
                [&](const std::vector<TargetPoint>& parsed) { points += parsed.size(); });
        });
    test::CheckSurvey(files, 5);
    CHECK(points == 6);
}

}
}
