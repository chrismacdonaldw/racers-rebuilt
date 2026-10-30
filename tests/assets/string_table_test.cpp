#include "assets/string_table.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"

namespace rr::assets {
namespace {

std::vector<std::byte> Units(std::initializer_list<std::uint16_t> units) {
    std::vector<std::byte> bytes;
    for (const std::uint16_t unit : units) {
        bytes.insert(bytes.end(),
                     {static_cast<std::byte>(unit & 0xFFU), static_cast<std::byte>(unit >> 8U)});
    }
    return bytes;
}

TEST_CASE("a string table reads each string up to its terminator") {
    const std::vector<std::byte> bytes =
        Units({3, 9, 0, 3, 4, 'G', 'O', 0, 0, 0xC6, '\n', 'B', 0, 0});

    const Result<std::vector<std::u16string>> strings = ParseStringTable(bytes);

    REQUIRE(strings.has_value());
    CHECK(*strings == std::vector<std::u16string>{u"GO", u"", u"\u00C6\nB"});
}

TEST_CASE("a string table with a bad offset or no terminator fails") {
    CHECK_FALSE(ParseStringTable(Units({1, 2, 2, 'A', 0})).has_value());
    CHECK_FALSE(ParseStringTable(Units({1, 2, 0, 'A', 'B'})).has_value());
    CHECK_FALSE(ParseStringTable(Units({2, 2, 0})).has_value());
}

TEST_CASE("every string table in the retail game parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    const test::Survey tables = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "SRF"; },
        [](std::span<const std::byte> bytes) {
            return ParseStringTable(bytes).transform([](const auto&) {});
        });
    test::CheckSurvey(tables, 181);

    const Result<std::vector<std::u16string>> names =
        test::ReadRetailFile(*archive, "MENUDATA/MENUNAME.SRF").and_then([](const FileData& data) {
            return ParseStringTable(data.Bytes());
        });
    REQUIRE(names.has_value());
    CHECK(std::ranges::find(*names, u"mainmenu") != names->end());
}

}
}
