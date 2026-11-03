#include "app/intro/soundtrack_clock.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>

namespace rr::app {
namespace {

constexpr Microseconds Ms(int milliseconds) {
    return Microseconds{std::int64_t{milliseconds} * 1000};
}

TEST_CASE("the soundtrack clock follows the soundtrack while it plays") {
    SoundtrackClock clock;

    CHECK(clock.Now(Ms(0), Milliseconds{0}) == Ms(0));
    CHECK(clock.Now(Ms(300), Milliseconds{100}) == Ms(100));
    CHECK(clock.Now(Ms(700), Milliseconds{100}) == Ms(100));
    CHECK(clock.Now(Ms(900), Milliseconds{650}) == Ms(650));
    CHECK_FALSE(clock.Now(Ms(1000), std::nullopt).has_value());
}

TEST_CASE("the soundtrack clock falls back to sim time once the soundtrack stalls") {
    SoundtrackClock clock;

    CHECK(clock.Now(Ms(0), Milliseconds{0}) == Ms(0));
    CHECK(clock.Now(Ms(500), Milliseconds{0}) == Ms(0));
    CHECK(clock.Now(Ms(512), Milliseconds{0}) == Ms(512));
    CHECK(clock.Now(Ms(600), Milliseconds{40}) == Ms(600));
    CHECK(clock.Now(Ms(700), std::nullopt) == Ms(700));
}

}
}
