#include "app/track_space.hpp"

#include <doctest/doctest.h>

#include <cmath>

namespace rr::app {
namespace {

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x).epsilon(1e-4));
    CHECK(actual.y == doctest::Approx(expected.y).epsilon(1e-4));
    CHECK(actual.z == doctest::Approx(expected.z).epsilon(1e-4));
}

Vector3 Direction(const Vector3& from, const Vector3& to) {
    const Vector3 d{.x = to.x - from.x, .y = to.y - from.y, .z = to.z - from.z};
    const float length = std::sqrt((d.x * d.x) + (d.y * d.y) + (d.z * d.z));
    return {.x = d.x / length, .y = d.y / length, .z = d.z / length};
}

TEST_CASE("game space turns into the engine's y-up space without mirroring") {
    CheckNear(ToEngine({.z = 1.0F}), {.y = 1.0F});
    CheckNear(ToEngine({.y = 1.0F}), {.z = -1.0F});
    CheckNear(ToEngine({.x = 1.0F}), {.x = 1.0F});
}

TEST_CASE("the default placement doesn't rotate") {
    const Transform placed =
        PlacementToEngine({.x = 1.0F, .y = 2.0F, .z = 3.0F}, {.x = 1.0F}, {.z = 1.0F});

    CheckNear(placed.position, {.x = 1.0F, .y = 3.0F, .z = -2.0F});
    CheckNear(Apply({.rotation = placed.rotation}, {.x = 1.0F}), {.x = 1.0F});
    CheckNear(Apply({.rotation = placed.rotation}, {.y = 1.0F}), {.y = 1.0F});
}

TEST_CASE("a placement turns the model's axes to its forward and up") {
    const Transform placed = PlacementToEngine({}, {.y = 4.0F}, {.y = 1.0F, .z = 2.0F});

    CheckNear(Apply(placed, ToEngine({.x = 1.0F})), ToEngine({.y = 1.0F}));
    CheckNear(Apply(placed, ToEngine({.z = 1.0F})), ToEngine({.z = 1.0F}));
    CheckNear(Apply(placed, ToEngine({.y = 1.0F})), ToEngine({.x = -1.0F}));
}

TEST_CASE("a degenerate placement keeps only its position") {
    const Transform placed = PlacementToEngine({.x = 5.0F}, {}, {.z = 1.0F});

    CHECK(placed.rotation == Quaternion{});
    CheckNear(placed.position, {.x = 5.0F});
}

TEST_CASE("a look-at camera faces its target and stays level") {
    const Vector3 eye{.x = 1.0F, .y = 5.0F, .z = 2.0F};
    const Vector3 target{.x = 4.0F, .y = 1.0F, .z = -3.0F};

    const Transform camera = LookAt(eye, target);

    CheckNear(camera.position, eye);
    CheckNear(Apply({.rotation = camera.rotation}, {.z = -1.0F}), Direction(eye, target));
    CHECK(Apply({.rotation = camera.rotation}, {.x = 1.0F}).y == doctest::Approx(0.0F));
}

}
}
