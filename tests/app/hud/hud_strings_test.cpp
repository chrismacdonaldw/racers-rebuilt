#include "app/hud/hud_strings.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <vector>

namespace rr::app {
namespace {

std::vector<std::u16string> NumberedStrings(std::size_t count) {
    std::vector<std::u16string> strings;
    strings.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        strings.emplace_back(1, static_cast<char16_t>(u'A' + i));
    }
    return strings;
}

TEST_CASE("HUD labels come from their game string slots") {
    const std::vector<std::u16string> game = NumberedStrings(0x2E);

    const Result<HudStrings> strings = ReadHudStrings(game);

    REQUIRE(strings.has_value());
    CHECK(strings->placeSuffixes.front() == game[0x19]);
    CHECK(strings->placeSuffixes.back() == game[0x20]);
    CHECK(strings->gap == game[0x21]);
    CHECK(strings->wrongWay == game[0x24]);
    CHECK(strings->best == game[0x25]);
    CHECK(strings->lap == game[0x27]);
    CHECK(strings->finish == game[0x28]);
    CHECK(strings->go == game[0x29]);
    CHECK(strings->finished == game[0x2A]);
    CHECK(strings->stillRacing == game[0x2B]);
    CHECK(strings->circuit == game[0x2D]);
}

TEST_CASE("pause labels come from their game string slots") {
    const std::vector<std::u16string> game = NumberedStrings(0x2E);

    const Result<HudStrings> strings = ReadHudStrings(game);

    REQUIRE(strings.has_value());
    CHECK(strings->paused == game[0x14]);
    CHECK(strings->resume == game[0x0E]);
    CHECK(strings->restart == game[0x0F]);
    CHECK(strings->restartCircuit == game[0x10]);
    CHECK(strings->quit == game[0x11]);
    CHECK(strings->sure == game[0x2C]);
    CHECK(strings->yes == game[0x12]);
    CHECK(strings->no == game[0x13]);
}

TEST_CASE("a game string table without every HUD label fails") {
    CHECK_FALSE(ReadHudStrings(NumberedStrings(0x2D)).has_value());
}

}
}
