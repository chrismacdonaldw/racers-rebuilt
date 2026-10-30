#include "app/hud/race_time_text.hpp"

#include <doctest/doctest.h>

#include <array>
#include <string>

namespace rr::app {
namespace {

std::u16string Text(std::int64_t ms) {
    std::array<char16_t, kRaceTimeLength> buffer{};
    return std::u16string(WriteRaceTime(Milliseconds{ms}, buffer));
}

TEST_CASE("race times read minutes, seconds and hundredths") {
    CHECK(Text(0) == u"0:00:00");
    CHECK(Text(83456) == u"1:23:45");
    CHECK(Text(599999) == u"9:59:99");
}

TEST_CASE("from ten minutes the minutes take two digits") {
    CHECK(Text(600000) == u"10:00:00");
    CHECK(Text(3599990) == u"59:59:99");
}

TEST_CASE("race times wrap every hour") {
    CHECK(Text(3600000 + 61230) == u"1:01:23");
}

}
}
