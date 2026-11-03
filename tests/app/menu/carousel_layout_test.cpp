#include "app/menu/carousel_layout.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <numbers>

#include "app/track_space.hpp"
#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr render::Rect kBounds{.x = 48.0F, .y = 28.0F, .width = 200.0F, .height = 64.0F};

assets::ModelCarouselElement PartBox(float distance = 2.0F) {
    return {.slotCount = 5,
            .focusedSlot = 2,
            .slots = {{.left = -40, .top = 8, .right = -5, .bottom = 64},
                      {.left = 5, .top = 8, .right = 60, .bottom = 64},
                      {.left = 65, .top = 0, .right = 140, .bottom = 72},
                      {.left = 145, .top = 8, .right = 200, .bottom = 64},
                      {.left = 205, .top = 8, .right = 240, .bottom = 64}},
            .camera = assets::CameraValues{0, 0, 0, 0, 0, 0, 4.0F, distance, 275.0F}};
}

float Unit() {
    return 2.0F * 2.0F * std::tan(2.0F * std::numbers::pi_v<float> / 180.0F) / 64.0F;
}

}

TEST_CASE("a carousel's slots sit across its view at the camera's distance") {
    const Result<CarouselLayout> layout = LayoutCarousel(PartBox(), kBounds);
    REQUIRE(layout.has_value());

    REQUIRE(layout->slots.size() == 5);
    CHECK(layout->focused == 2);
    CHECK(layout->slots[2].across == doctest::Approx(Unit() * 2.5F));
    CHECK(layout->slots[2].fit == doctest::Approx(Unit() * 72.0F));
    CHECK(layout->slots[1].across == doctest::Approx(Unit() * -67.5F));
    CHECK(layout->slots[1].fit == doctest::Approx(Unit() * 55.0F));
}

TEST_CASE("a carousel item sits behind the near plane, scaled out to keep its slot on screen") {
    const Result<CarouselLayout> layout = LayoutCarousel(PartBox(), kBounds);
    REQUIRE(layout.has_value());
    const BoundingSphere bounds{.centre = {.x = 0.1F, .y = 0.0F, .z = 0.3F}, .radius = 0.01F};

    const Vector3 position = SlotPosition(*layout, 1, bounds);

    const float depth = -(0.02F + 2.0F);
    CHECK(position.x == doctest::Approx(depth - 0.1F));
    CHECK(position.y == doctest::Approx(layout->slots[1].across * (2.0F - depth) / 2.0F));
    CHECK(position.z == doctest::Approx(-0.3F));
}

TEST_CASE("a carousel item too big for its slot goes further back") {
    const Result<CarouselLayout> layout = LayoutCarousel(PartBox(), kBounds);
    REQUIRE(layout.has_value());
    const float fit = layout->slots[2].fit;

    const Vector3 position = SlotPosition(*layout, 2, {.radius = fit * 4.0F});

    const float diameter = fit * 8.0F;
    CHECK(position.x ==
          doctest::Approx(std::min(-(2.0F / fit) * (diameter - fit), -(diameter + 2.0F))));
}

TEST_CASE("a carousel camera looks back down game X with Z up") {
    const Result<CarouselLayout> layout = LayoutCarousel(PartBox(), kBounds);
    REQUIRE(layout.has_value());

    const render::Camera camera = CarouselCamera(*layout);

    CHECK(camera.nearPlane == doctest::Approx(2.0F));
    CHECK(camera.farPlane == doctest::Approx(275.0F));
    CHECK(camera.verticalFovDegrees == doctest::Approx(4.0F));
    const Vector3 forward = ToGame(Rotate(camera.transform.rotation, {.z = -1.0F}));
    CHECK(forward.x == doctest::Approx(-1.0F));
    const Vector3 right = ToGame(Rotate(camera.transform.rotation, {.x = 1.0F}));
    CHECK(right.y == doctest::Approx(1.0F));
}

TEST_CASE("a carousel without a camera or camera distance has no layout") {
    CHECK_FALSE(LayoutCarousel(PartBox(0.0F), kBounds).has_value());

    assets::ModelCarouselElement carousel = PartBox();
    carousel.camera.reset();
    CHECK_FALSE(LayoutCarousel(carousel, kBounds).has_value());
}

}
