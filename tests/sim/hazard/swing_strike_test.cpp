#include "sim/hazard/swing_strike.hpp"

#include <doctest/doctest.h>

namespace rr::sim {
namespace {

constexpr StrikeWindows kWindows{
    .strike = {{{.after = 20.0F, .before = 30.0F}, {.after = 70.0F, .before = 80.0F}}},
    .rearm = {{{.after = 0.0F, .before = 10.0F}, {.after = 40.0F, .before = 60.0F}}}};

TEST_CASE("an armed swing strikes once, then re-arms only in a re-arm window") {
    bool isArmed = true;

    CHECK(Swing(kWindows, 25.0F, isArmed));
    CHECK_FALSE(isArmed);
    CHECK_FALSE(Swing(kWindows, 75.0F, isArmed));
    CHECK_FALSE(Swing(kWindows, 50.0F, isArmed));
    CHECK(isArmed);
    CHECK(Swing(kWindows, 75.0F, isArmed));
}

TEST_CASE("window edges are outside the window") {
    bool isArmed = true;

    CHECK_FALSE(Swing(kWindows, 20.0F, isArmed));
    CHECK_FALSE(Swing(kWindows, 30.0F, isArmed));
    CHECK(isArmed);
}

}
}
