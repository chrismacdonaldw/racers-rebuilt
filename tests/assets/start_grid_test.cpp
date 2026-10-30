#include "assets/start_grid.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
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

std::vector<std::byte> Slot(std::uint8_t index, std::int16_t x) {
    return Join({Bytes({0x27}), Ints({index}), Bytes({0x05, 0x28}), Floats({x, 5, 0}),
                 Bytes({0x29}), Floats({1, 0, 0, 0, 0, 1}), Bytes({0x06})});
}

std::vector<std::byte> File(std::initializer_list<std::vector<std::byte>> slots) {
    std::vector<std::byte> bytes =
        Join({Bytes({0x27}), test::ListHeader(static_cast<std::uint8_t>(slots.size()))});
    for (const std::vector<std::byte>& slot : slots) {
        bytes.insert(bytes.end(), slot.begin(), slot.end());
    }
    bytes.push_back(std::byte{0x06});
    return bytes;
}

TEST_CASE("a start grid puts each slot at its index, whatever the file order") {
    const Result<StartGridAsset> grid = ParseSpb(File({Slot(1, 20), Slot(0, 10)}));

    REQUIRE(grid.has_value());
    REQUIRE(grid->slots.size() == 2);
    CHECK(grid->slots[0].position == Vector3{10, 5, 0});
    CHECK(grid->slots[1].position == Vector3{20, 5, 0});
    CHECK(grid->slots[1].forward == Vector3{1, 0, 0});
    CHECK(grid->slots[1].up == Vector3{0, 0, 1});
}

TEST_CASE("a start grid rejects a repeated or out-of-range slot") {
    CHECK_FALSE(ParseSpb(File({Slot(0, 10), Slot(0, 20)})).has_value());
    CHECK_FALSE(ParseSpb(File({Slot(0, 10), Slot(2, 20)})).has_value());
}

TEST_CASE("a start grid rejects a slot count past 255 before reading any slot") {
    const std::vector<std::byte> huge =
        Bytes({0x27, 0x07, 0x04, 0x00, 0x00, 0x00, 0x01, 0x08, 0x05, 0x06});

    CHECK_FALSE(ParseSpb(huge).has_value());
}

TEST_CASE("a start grid rejects a slot missing its axes") {
    const std::vector<std::byte> noAxes =
        Join({Bytes({0x27}), Ints({0}), Bytes({0x05, 0x28}), Floats({0, 0, 0}), Bytes({0x06})});

    CHECK_FALSE(ParseSpb(File({noAxes})).has_value());
}

TEST_CASE("every start grid in the retail game parses, with 6 slots on race tracks") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    std::map<std::size_t, std::size_t> gridsBySize;
    const test::Survey files = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "SPB"; },
        [&](std::span<const std::byte> bytes) {
            return ParseSpb(bytes).transform(
                [&](const StartGridAsset& grid) { ++gridsBySize[grid.slots.size()]; });
        });
    test::CheckSurvey(files, 14);
    CHECK(gridsBySize == std::map<std::size_t, std::size_t>{{1, 1}, {6, 13}});
}

}
}
