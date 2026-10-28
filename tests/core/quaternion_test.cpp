#include "core/quaternion.hpp"

#include <doctest/doctest.h>

#include <numbers>

namespace rr {
namespace {

TEST_CASE("nlerp blends along the shorter arc") {
    const Quaternion identity{};
    const Quaternion negatedIdentity{.w = -1.0F};

    const Quaternion blended = Nlerp(identity, negatedIdentity, 0.5F);

    CHECK(blended.w == doctest::Approx(1.0F));
}

TEST_CASE("nlerp returns a unit quaternion") {
    const Quaternion identity{};
    const Quaternion quarterTurn{.w = 0.70710678F, .y = 0.70710678F};

    const Quaternion blended = Nlerp(identity, quarterTurn, 0.5F);

    CHECK(Dot(blended, blended) == doctest::Approx(1.0F));
}

TEST_CASE("an axis-angle turn rotates about its axis") {
    const Quaternion halfTurn = FromAxisAngle({.y = 1.0F}, std::numbers::pi_v<float>);

    CHECK(halfTurn.w == doctest::Approx(0.0F));
    CHECK(halfTurn.y == doctest::Approx(1.0F));
    CHECK(Dot(halfTurn, halfTurn) == doctest::Approx(1.0F));
}

}
}
