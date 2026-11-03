#include "app/hud/place_pulse.hpp"

#include <doctest/doctest.h>

namespace rr::app {
namespace {

constexpr Milliseconds kQuarter{87};
constexpr Milliseconds kTick{12};

TEST_CASE("the race opens with a pulse from place 9 to the starting place") {
    PlacePulse pulse;
    CHECK(pulse.Shown() == 9);

    pulse.Advance(6, kTick);
    pulse.Advance(6, kQuarter);

    CHECK(pulse.Shown() == 6);
    CHECK(pulse.Scale() == doctest::Approx(1.497F).epsilon(0.01));
}

TEST_CASE("a new place swells to double size at 175 ms and settles by 350 ms") {
    PlacePulse pulse;
    pulse.Advance(3, kTick);
    const float start = pulse.Scale();

    pulse.Advance(3, kQuarter);
    const float growing = pulse.Scale();
    pulse.Advance(3, Milliseconds{88});
    const float peak = pulse.Scale();
    pulse.Advance(3, Milliseconds{175});

    CHECK(start == doctest::Approx(1.0F));
    CHECK(growing == doctest::Approx(1.497F).epsilon(0.01));
    CHECK(peak == doctest::Approx(2.0F));
    CHECK(pulse.Scale() == doctest::Approx(1.0F));
}

TEST_CASE("the shown place holds for 1050 ms, then takes the latest place with a new pulse") {
    PlacePulse pulse;
    pulse.Advance(3, kTick);
    pulse.Advance(2, Milliseconds{500});
    pulse.Advance(1, Milliseconds{500});
    CHECK(pulse.Shown() == 3);
    CHECK(pulse.Scale() == doctest::Approx(1.0F));

    pulse.Advance(1, Milliseconds{50});
    pulse.Advance(1, kQuarter);

    CHECK(pulse.Shown() == 1);
    CHECK(pulse.Scale() == doctest::Approx(1.497F).epsilon(0.01));
}

TEST_CASE("a place that doesn't change after the hold doesn't pulse") {
    PlacePulse pulse;
    pulse.Advance(4, kTick);
    pulse.Advance(4, Milliseconds{1100});

    pulse.Advance(4, kQuarter);

    CHECK(pulse.Scale() == doctest::Approx(1.0F));
}

}
}
