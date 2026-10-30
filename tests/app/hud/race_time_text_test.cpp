#include "app/hud/race_time_text.hpp"

#include <doctest/doctest.h>

namespace rr::app {
namespace {

TEST_CASE("race times read minutes, seconds and hundredths") {
    CHECK(RaceTimeText(Milliseconds{0}) == u"0:00:00");
    CHECK(RaceTimeText(Milliseconds{83456}) == u"1:23:45");
    CHECK(RaceTimeText(Milliseconds{599999}) == u"9:59:99");
}

TEST_CASE("from ten minutes the minutes take two digits") {
    CHECK(RaceTimeText(Milliseconds{600000}) == u"10:00:00");
    CHECK(RaceTimeText(Milliseconds{3599990}) == u"59:59:99");
}

TEST_CASE("race times wrap every hour") {
    CHECK(RaceTimeText(Milliseconds{3600000 + 61230}) == u"1:01:23");
}

}
}
