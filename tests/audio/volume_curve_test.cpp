#include "audio/volume_curve.hpp"

#include <doctest/doctest.h>

namespace rr::audio {
namespace {

TEST_CASE("volume falls 30 dB from full to the quietest audible level") {
    CHECK(VolumeGain(1.0F) == doctest::Approx(1.0F));
    CHECK(VolumeGain(0.5F) == doctest::Approx(0.17783F).epsilon(0.001));
    CHECK(VolumeGain(0.005F) == doctest::Approx(0.03217F).epsilon(0.001));
}

TEST_CASE("volume below the audible level is silent") {
    CHECK(VolumeGain(0.0F) == 0.0F);
    CHECK(VolumeGain(0.0049F) == 0.0F);
}

TEST_CASE("volume past full stays at full") {
    CHECK(VolumeGain(1.2F) == doctest::Approx(1.0F));
}

TEST_CASE("pan cuts the far channel by up to 25 dB") {
    CHECK(PanBalance(0.0F) == doctest::Approx(0.0F));
    CHECK(PanBalance(0.7F) == doctest::Approx(1.0F - 0.13335F).epsilon(0.001));
    CHECK(PanBalance(-0.7F) == doctest::Approx(-1.0F + 0.13335F).epsilon(0.001));
}

TEST_CASE("pan fully to one side silences the other") {
    CHECK(PanBalance(1.0F) == 1.0F);
    CHECK(PanBalance(-1.0F) == -1.0F);
}

}
}
