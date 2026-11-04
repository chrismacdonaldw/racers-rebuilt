#include "app/menu/workshop/workshop_scenery.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "app/track_space.hpp"
#include "render/camera.hpp"

namespace rr::app {
namespace {

TEST_CASE("a ground cell is faint inside and brighter at its edges") {
    const assets::ImageAsset cell = GroundCellImage();
    const auto alphaAt = [&](std::uint32_t x, std::uint32_t y) {
        return std::to_integer<int>(cell.rgba.at((((y * cell.widthPx) + x) * 4) + 3));
    };

    REQUIRE(cell.rgba.size() == static_cast<std::size_t>(cell.widthPx) * cell.heightPx * 4);
    CHECK(alphaAt(0, 5) > alphaAt(5, 5));
    CHECK(alphaAt(5, cell.heightPx - 1) > alphaAt(5, 5));
    CHECK(alphaAt(5, 5) > 0);
}

TEST_CASE("the backdrop fills the view behind everything") {
    const render::Camera camera{.farPlane = 100.0F};

    const render::QuadDraw backdrop = BackdropQuad(camera, {.r = 0.5F, .g = 0.6F, .b = 1.0F});

    CHECK(std::ranges::all_of(backdrop.corners, [](const Vector3& corner) {
        return std::abs(corner.z + 95.0F) < 0.001F;
    }));
    CHECK(backdrop.corners[0].y > backdrop.corners[2].y);
    CHECK(backdrop.corners[0].x < backdrop.corners[1].x);
}

TEST_CASE("the backdrop darkens toward the bottom") {
    const render::QuadDraw backdrop =
        BackdropQuad(render::Camera{}, {.r = 0.5F, .g = 0.6F, .b = 1.0F});

    CHECK(backdrop.cornerTints[0].b == doctest::Approx(1.0F));
    CHECK(backdrop.cornerTints[2].b < backdrop.cornerTints[0].b);
    CHECK(backdrop.blend == render::BlendMode::Opaque);
}

TEST_CASE("the ground repeats its cell once per grid cell, flat on the ground") {
    const render::QuadDraw ground =
        GroundQuad(render::TextureId{3}, {.x = -2.0F, .y = 1.0F}, 32.0F, 16.0F);

    CHECK(ground.uv.width == doctest::Approx(32.0F));
    CHECK(ground.uv.height == doctest::Approx(16.0F));
    CHECK(ground.corners[2] == ToEngine({.x = -2.0F, .y = 1.0F}));
    CHECK(ground.corners[1] == ToEngine({.x = 30.0F, .y = 17.0F}));
    CHECK(ground.sampling.address == render::TextureAddress::Repeat);
}

}
}
