#include "core/result.hpp"

#include <doctest/doctest.h>

namespace rr {
namespace {

Result<int> ParsePositive(int value) {
    if (value <= 0) {
        return Fail(ErrorCode::InvalidData, "value must be positive");
    }
    return value;
}

TEST_CASE("a successful result holds its value") {
    Result<int> result = ParsePositive(3);

    REQUIRE(result.has_value());
    CHECK(*result == 3);
}

TEST_CASE("a failed result carries its code and message") {
    Result<int> result = ParsePositive(-1);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == ErrorCode::InvalidData);
    CHECK(result.error().message == "value must be positive");
}

TEST_CASE("every error code has a readable name") {
    CHECK(ToString(ErrorCode::NotFound) == "not found");
    CHECK(ToString(ErrorCode::Io) == "i/o");
    CHECK(ToString(ErrorCode::InvalidData) == "invalid data");
    CHECK(ToString(ErrorCode::Platform) == "platform");
}

}
}
