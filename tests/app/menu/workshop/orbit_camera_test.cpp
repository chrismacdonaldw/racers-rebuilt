#include "app/menu/workshop/orbit_camera.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <numbers>

#include "core/duration.hpp"
#include "core/vector3.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kCar{.x = 1.0F, .y = 2.0F, .z = 1.0F};

OrbitCamera Settled(OrbitCamera camera) {
    camera.Tick(Milliseconds{2000});
    return camera;
}

TEST_CASE("the camera eases toward its goal and settles there") {
    OrbitCamera camera({.focus = kCar, .yaw = 0.0F, .pitch = 0.5F, .distance = 10.0F});
    camera.Zoom(-2.0F);

    camera.Tick(Milliseconds{12});
    const float partway = Length(camera.Eye() - camera.Focus());
    camera = Settled(camera);

    CHECK(partway > 10.0F);
    CHECK(partway < camera.Goal().distance);
    CHECK(Length(camera.Eye() - camera.Focus()) == doctest::Approx(10.0F / (0.85F * 0.85F)));
}

TEST_CASE("the camera stays above the ground and within reach") {
    OrbitCamera camera({.focus = kCar, .yaw = 0.0F, .pitch = 0.5F, .distance = 10.0F});

    camera.Turn(0.0F, -5.0F);
    CHECK(camera.Goal().pitch > 0.0F);
    camera.Turn(0.0F, 10.0F);
    CHECK(camera.Goal().pitch < std::numbers::pi_v<float> / 2.0F);
    camera.Zoom(100.0F);
    CHECK(camera.Goal().distance >= 3.0F);
    camera.Pan(1000.0F, -1000.0F);
    CHECK(std::abs(camera.Goal().focus.y) <= 24.0F);
    CHECK(camera.Goal().focus.z >= 0.0F);
}

TEST_CASE("a pan drags the scene the way the pointer goes") {
    OrbitCamera camera({.focus = kCar, .yaw = 0.0F, .pitch = 0.05F, .distance = 10.0F});

    camera.Pan(0.1F, 0.0F);

    CHECK(camera.Goal().focus.y == doctest::Approx(kCar.y - 1.0F));
    CHECK(camera.Goal().focus.x == doctest::Approx(kCar.x));
}

TEST_CASE("framing fits the sphere and home goes back to the start") {
    OrbitCamera camera({.focus = kCar, .yaw = 0.3F, .pitch = 0.5F, .distance = 10.0F});
    camera.Turn(1.0F, 0.2F);

    camera.Frame({.centre = {.x = 4.0F}, .radius = 5.0F}, 60.0F);
    CHECK(camera.Goal().focus == Vector3{.x = 4.0F});
    CHECK(camera.Goal().distance == doctest::Approx(11.5F));
    CHECK(camera.Goal().yaw == doctest::Approx(1.3F));

    camera.Home();
    camera = Settled(camera);
    CHECK(camera.Focus().y == doctest::Approx(kCar.y));
    CHECK(camera.Eye().x == doctest::Approx(kCar.x + (10.0F * std::cos(0.5F) * std::cos(0.3F))));
}

}
}
