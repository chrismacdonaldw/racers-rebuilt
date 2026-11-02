#include "assets/original_save.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <vector>

#include "assets/save_bytes.hpp"
#include "game_files.hpp"

namespace rr::assets {
namespace {

std::vector<std::byte> ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::vector<std::byte> bytes;
    for (auto at = std::istreambuf_iterator<char>(file); at != std::istreambuf_iterator<char>();
         ++at) {
        bytes.push_back(static_cast<std::byte>(*at));
    }
    return bytes;
}

test::SaveBytes Played() {
    test::SaveBytes save;
    save.Byte(0x005, 0b101).Byte(0x006, 0b11).U16(0x007, 0x0804);
    save.Time(0x009 + (2 * 0x40), 31250, u"KING KAHUKA");
    save.Time(0x009 + (2 * 0x40) + 0x20, 98000, u"ABCDEFGHIJKLMN");
    save.Driver(0b0111'0001).Driver(0b10);
    return save;
}

TEST_CASE("a save reads its unlocks, times and each driver's trophies") {
    const Result<OriginalSave> save = ParseOriginalSave(Played().File());

    REQUIRE(save.has_value());
    OriginalSave expected{.partSets = 0b101,
                          .circuits = 0b11,
                          .beatenRecords = 0x0804,
                          .bestLaps = {},
                          .bestRaces = {},
                          .driverTrophies = {0b0111'0001, 0b10}};
    expected.bestLaps.at(2) = {.ms = 31250, .holder = u"KING KAHUKA"};
    expected.bestRaces.at(2) = {.ms = 98000, .holder = u"ABCDEFGHIJKLMN"};
    CHECK(*save == expected);
}

TEST_CASE("a save with a bad checksum, a stray byte or no LR fails") {
    std::vector<std::byte> bad = Played().File();
    bad[1300] ^= std::byte{1};
    CHECK_FALSE(ParseOriginalSave(bad).has_value());

    std::vector<std::byte> stray = Played().File();
    stray.push_back(std::byte{0});
    CHECK_FALSE(ParseOriginalSave(stray).has_value());

    CHECK_FALSE(ParseOriginalSave(test::SaveBytes().Byte(0, 'X').File()).has_value());
    CHECK_FALSE(ParseOriginalSave(test::SaveBytes().Byte(2, 1).File()).has_value());
}

TEST_CASE("the retail saves hold one empty driver and none") {
    const std::optional<std::filesystem::path> game = test::GameFolder();
    if (!game) {
        return;
    }

    const Result<OriginalSave> first = ParseOriginalSave(ReadFile(*game / "Save/0/LEGORac1"));
    const Result<OriginalSave> second = ParseOriginalSave(ReadFile(*game / "Save/1/LEGORac1"));

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(*first == OriginalSave{.driverTrophies = {0}});
    CHECK(*second == OriginalSave{});
}

}
}
