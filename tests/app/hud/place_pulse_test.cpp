#include "app/hud/place_pulse.hpp"

#include <doctest/doctest.h>

namespace rr::app {
namespace {

constexpr Milliseconds kQuarter{87};

TEST_CASE("the starting place doesn't pulse") {
    PlacePulse pulse;
    pulse.Advance(6, Milliseconds{12});

    pulse.Advance(6, kQuarter);

    CHECK(pulse.Scale() == doctest::Approx(1.0F));
}

TEST_CASE("a new place swells to double size at 175 ms and settles by 350 ms") {
    PlacePulse pulse;
    pulse.Advance(4, Milliseconds{12});
    pulse.Advance(3, Milliseconds{12});
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

TEST_CASE("a pulse ends after 350 ms") {
    PlacePulse pulse;
    pulse.Advance(3, Milliseconds{12});
    pulse.Advance(2, Milliseconds{12});
    pulse.Advance(2, Milliseconds{400});

    pulse.Advance(2, kQuarter);

    CHECK(pulse.Scale() == doctest::Approx(1.0F));
}

TEST_CASE("a place change mid-pulse starts the pulse again") {
    PlacePulse pulse;
    pulse.Advance(3, Milliseconds{12});
    pulse.Advance(2, Milliseconds{12});
    pulse.Advance(2, Milliseconds{175});

    pulse.Advance(1, Milliseconds{12});
    pulse.Advance(1, kQuarter);

    CHECK(pulse.Scale() == doctest::Approx(1.497F).epsilon(0.01));
}

}
}
