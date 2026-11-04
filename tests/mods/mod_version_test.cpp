#include "mods/mod_version.hpp"

#include <doctest/doctest.h>

#include <string_view>

namespace rr::mods {
namespace {

VersionRange RangeOf(std::string_view text) {
    const Result<VersionRange> range = ParseVersionRange(text);
    REQUIRE(range.has_value());
    return range.value_or(VersionRange{});
}

TEST_CASE("a mod version is three whole numbers") {
    CHECK(ParseModVersion("1.12.0") == ModVersion{.major = 1, .minor = 12, .patch = 0});
    for (const std::string_view text :
         {"1.0", "1.0.0.0", "1.x.0", "-1.0.0", "+1.0.0", "", "1..0"}) {
        CHECK_FALSE(ParseModVersion(text).has_value());
    }
}

TEST_CASE("version ranges match the versions they name") {
    CHECK(Matches(RangeOf("*"), "anything"));
    CHECK(Matches(RangeOf("=1.2.0"), "1.2.0"));
    CHECK_FALSE(Matches(RangeOf("=1.2.0"), "1.2.1"));
    CHECK(Matches(RangeOf(">=1.2.0"), "2.0.0"));
    CHECK_FALSE(Matches(RangeOf(">=1.2.0"), "1.1.9"));
    CHECK(Matches(RangeOf("^1.2.0"), "1.9.0"));
    CHECK_FALSE(Matches(RangeOf("^1.2.0"), "2.0.0"));
    CHECK_FALSE(Matches(RangeOf("^1.2.0"), "1.1.0"));
}

TEST_CASE("a version that isn't x.y.z only matches any version") {
    CHECK(Matches(RangeOf("*"), "1.0"));
    CHECK_FALSE(Matches(RangeOf(">=0.0.0"), "1.0"));
}

TEST_CASE("a range that isn't one of the four forms is an error") {
    for (const std::string_view text : {"1.2.0", ">1.2.0", "~1.2.0", ">=1.2", "**"}) {
        CHECK_FALSE(ParseVersionRange(text).has_value());
    }
}

TEST_CASE("ranges describe themselves for the Mods menu") {
    CHECK(DescribeRange(RangeOf("*")).empty());
    CHECK(DescribeRange(RangeOf("=1.2.0")) == "1.2.0");
    CHECK(DescribeRange(RangeOf(">=1.2.0")) == "1.2.0 or newer");
    CHECK(DescribeRange(RangeOf("^1.2.0")) == "1.2.0 or a newer 1.x");
}

}
}
