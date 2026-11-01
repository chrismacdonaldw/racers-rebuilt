#include "core/transform.hpp"

#include <doctest/doctest.h>

#include <numbers>

namespace rr {
namespace {

constexpr float kQuarterTurn = std::numbers::pi_v<float> / 2.0F;

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x));
    CHECK(actual.y == doctest::Approx(expected.y));
    CHECK(actual.z == doctest::Approx(expected.z));
}

TEST_CASE("a transform rotates a point, then moves it") {
    const Transform turned{.position = {.z = 2.0F},
                           .rotation = FromAxisAngle({.z = 1.0F}, kQuarterTurn)};

    CheckNear(Apply(turned, {.x = 1.0F}), {.y = 1.0F, .z = 2.0F});
}

TEST_CASE("a composed transform applies the child first") {
    const Transform parent{.position = {.x = 5.0F},
                           .rotation = FromAxisAngle({.z = 1.0F}, kQuarterTurn)};
    const Transform child{.position = {.x = 1.0F},
                          .rotation = FromAxisAngle({.x = 1.0F}, kQuarterTurn)};

    const Transform composed = Compose(parent, child);

    CheckNear(composed.position, {.x = 5.0F, .y = 1.0F});
    CheckNear(Apply(composed, {.y = 1.0F}), Apply(parent, Apply(child, {.y = 1.0F})));
}

TEST_CASE("a transform composed with its inverse leaves a point where it was") {
    const Transform placed{.position = {.x = 3.0F, .y = -2.0F, .z = 1.0F},
                           .rotation = FromAxisAngle({.y = 1.0F}, 0.4F)};
    const Vector3 point{.x = 1.0F, .y = 2.0F, .z = 3.0F};

    CheckNear(Apply(Compose(Inverse(placed), placed), point), point);
    CheckNear(Apply(Compose(placed, Inverse(placed)), point), point);
}

TEST_CASE("rotating back by the same angle restores a vector") {
    const Vector3 axis{.y = 1.0F};

    CheckNear(Rotate(FromAxisAngle(axis, -0.7F),
                     Rotate(FromAxisAngle(axis, 0.7F), {.x = 1.0F, .z = 2.0F})),
              {.x = 1.0F, .z = 2.0F});
}

}
}
