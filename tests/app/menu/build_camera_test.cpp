#include "app/menu/build_camera.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <numbers>

namespace rr::app {
namespace {

constexpr float kQuarter = std::numbers::pi_v<float> / 4.0F;
constexpr float kFirstAngle = 1.57F;

bool SameEye(const Vector3& eye, const Vector3& expected) {
    return eye.x == doctest::Approx(expected.x) && eye.y == doctest::Approx(expected.y) &&
           eye.z == doctest::Approx(expected.z);
}

}

TEST_CASE("a turn eases to the next slot and ignores turns until it settles") {
    BuildCamera camera;
    REQUIRE(camera.Turn(1));
    CHECK_FALSE(camera.Turn(1));
    CHECK(camera.Slot() == 2);

    camera.Tick(Milliseconds{75});
    CHECK(camera.Angle() > kFirstAngle + kQuarter);
    CHECK(camera.Angle() < kFirstAngle + (2.0F * kQuarter));

    camera.Tick(Milliseconds{75});
    CHECK(camera.Angle() == doctest::Approx(kFirstAngle + (2.0F * kQuarter)));
    CHECK(camera.Turn(1));
}

TEST_CASE("turning back past the first slot wraps the short way round") {
    BuildCamera camera;
    camera.Turn(-1);
    camera.Tick(Milliseconds{150});
    camera.Turn(-1);
    CHECK(camera.Slot() == 7);

    camera.Tick(Milliseconds{75});
    CHECK(camera.Angle() < kFirstAngle);
    CHECK(camera.Angle() > kFirstAngle - kQuarter);
}

TEST_CASE("a tilt stops at the overhead and lowest views") {
    BuildCamera camera;
    REQUIRE(camera.Tilt(1));
    CHECK_FALSE(camera.Tilt(-1));
    camera.Tick(Milliseconds{300});
    CHECK(camera.IsOverhead());
    CHECK_FALSE(camera.Tilt(1));

    camera.Tilt(-2);
    camera.Tick(Milliseconds{300});
    CHECK_FALSE(camera.Tilt(-1));
    CHECK(SameEye(camera.Eye(0.0F), {.y = -14.0F, .z = 9.0F}));
}

TEST_CASE("the eye eases between tilts and pulls back for a bigger car") {
    BuildCamera camera;
    CHECK(SameEye(camera.Eye(5.9F), {.y = -10.0F, .z = 14.0F}));
    CHECK(SameEye(camera.Eye(7.2F), {.y = -11.0F, .z = 15.5F}));
    CHECK(SameEye(camera.Eye(20.0F), {.y = -12.0F, .z = 17.0F}));

    camera.Tilt(1);
    camera.Tick(Milliseconds{150});
    const Vector3 halfway = camera.Eye(5.9F);
    CHECK(halfway.z > 14.0F);
    CHECK(halfway.z < 17.0F);
}

}
