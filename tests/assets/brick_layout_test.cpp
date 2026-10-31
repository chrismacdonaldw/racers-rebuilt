#include "assets/brick_layout.hpp"

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

std::vector<std::byte> Brick(std::uint8_t list, std::int16_t x,
                             std::initializer_list<std::uint8_t> extra = {}) {
    return Join({Bytes({list, 0x05, 0x28}), Floats({x, 5, 1}), Bytes(extra), Bytes({0x06})});
}

std::vector<std::byte> List(std::uint8_t field,
                            std::initializer_list<std::vector<std::byte>> items) {
    return Join({Bytes({field}), test::ListHeader(static_cast<std::uint8_t>(items.size())),
                 Join(items), Bytes({0x06})});
}

TEST_CASE("a brick file reads coloured bricks by kind tag and white bricks by position") {
    const std::vector<std::byte> file =
        Join({List(0x27, {Brick(0x27, 1, {0x2A}), Brick(0x27, 2, {0x2B}), Brick(0x27, 3, {0x2C}),
                          Brick(0x27, 4, {0x2D}), Brick(0x27, 5)}),
              List(0x2F, {Brick(0x2F, 6), Brick(0x2F, 7)})});

    const Result<BrickLayout> layout = ParsePwb(file);

    REQUIRE(layout.has_value());
    REQUIRE(layout->coloured.size() == 5);
    CHECK(layout->coloured[0].colour == BrickColour::Red);
    CHECK(layout->coloured[1].colour == BrickColour::Yellow);
    CHECK(layout->coloured[2].colour == BrickColour::Blue);
    CHECK(layout->coloured[3].colour == BrickColour::Green);
    CHECK(layout->coloured[4].colour == BrickColour::Green);
    CHECK(layout->coloured[0].position == Vector3{1, 5, 1});
    CHECK(layout->coloured[0].respawn == kDefaultBrickRespawn);
    CHECK(layout->white == std::vector<Vector3>{{6, 5, 1}, {7, 5, 1}});
}

TEST_CASE("a coloured brick takes its respawn time and skips the fields no retail file uses") {
    const std::vector<std::byte> extras =
        Join({Bytes({0x2E}), Ints({200}), Bytes({0x30}), test::Text("x"), Ints({1}), Bytes({0x31}),
              Ints({2})});
    const std::vector<std::byte> brick =
        Join({Bytes({0x27, 0x05, 0x28}), Floats({0, 0, 0}), extras, Bytes({0x06})});
    const std::vector<std::byte> file =
        Join({Bytes({0x27}), test::ListHeader(1), brick, Bytes({0x06}), List(0x2F, {})});

    const Result<BrickLayout> layout = ParsePwb(file);

    REQUIRE(layout.has_value());
    REQUIRE(layout->coloured.size() == 1);
    CHECK(layout->coloured[0].respawn == Milliseconds{200});
}

TEST_CASE("a brick file rejects a brick without a position, an unknown field or a missing list") {
    const std::vector<std::byte> noPosition = Bytes({0x27, 0x05, 0x2A, 0x06});
    const std::vector<std::byte> unknown = Brick(0x27, 1, {0x40});

    CHECK_FALSE(ParsePwb(Join({Bytes({0x27}), test::ListHeader(1), noPosition, Bytes({0x06}),
                               List(0x2F, {})}))
                    .has_value());
    CHECK_FALSE(ParsePwb(Join({List(0x27, {unknown}), List(0x2F, {})})).has_value());
    CHECK_FALSE(ParsePwb(List(0x27, {Brick(0x27, 1)})).has_value());
}

TEST_CASE("every brick file in the retail game parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    std::size_t coloured = 0;
    std::size_t white = 0;
    const test::Survey files = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "PWB"; },
        [&](std::span<const std::byte> bytes) {
            return ParsePwb(bytes).transform([&](const BrickLayout& layout) {
                coloured += layout.coloured.size();
                white += layout.white.size();
            });
        });
    test::CheckSurvey(files, 26);
    CHECK(coloured == 419);
    CHECK(white == 260);
}

}
}
