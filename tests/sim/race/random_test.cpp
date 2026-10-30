#include "sim/race/random.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>

namespace rr::sim {
namespace {

std::array<std::uint32_t, 3> FirstThree(std::uint64_t seed) {
    Random random(seed);
    return {random.Next(), random.Next(), random.Next()};
}

TEST_CASE("a seed gives the same pcg32 sequence every time") {
    CHECK(FirstThree(42) == std::array<std::uint32_t, 3>{3270867926U, 1795671209U, 1924641435U});
    CHECK(FirstThree(0) == std::array<std::uint32_t, 3>{3894649422U, 2055130073U, 2315086854U});
}

TEST_CASE("values below a bound cover the whole range and nothing past it") {
    Random random(7);
    std::array<int, 6> counts{};

    for (int i = 0; i < 600; ++i) {
        const std::uint32_t value = random.Below(6);
        REQUIRE(value < 6);
        ++counts.at(value);
    }

    for (const int count : counts) {
        CHECK(count > 0);
    }
}

}
}
