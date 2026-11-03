#include "app/menu/menu_backdrop.hpp"

#include <doctest/doctest.h>

#include <vector>

namespace rr::app {
namespace {

constexpr render::TextureId kTexture{7};
constexpr render::Rect kHole{.width = 200.0F, .height = 100.0F};
constexpr render::Camera kCamera{
    .verticalFovDegrees = 90.0F, .nearPlane = 1.0F, .farPlane = 100.0F};

void CheckCorner(const Vector3& corner, const Vector3& expected) {
    CHECK(corner.x == doctest::Approx(expected.x));
    CHECK(corner.y == doctest::Approx(expected.y));
    CHECK(corner.z == doctest::Approx(expected.z));
}

}

TEST_CASE("a picture filling the hole fills the camera's view just short of the far plane") {
    std::vector<render::QuadDraw> quads;
    AppendBackdropQuads(std::vector{MenuPicture{.texture = kTexture, .bounds = kHole}}, kHole,
                        kCamera, quads);

    REQUIRE(quads.size() == 1);
    CHECK(quads[0].texture == kTexture);
    CheckCorner(quads[0].corners[0], {.x = -198.0F, .y = 99.0F, .z = -99.0F});
    CheckCorner(quads[0].corners[3], {.x = 198.0F, .y = -99.0F, .z = -99.0F});
}

TEST_CASE("only the part of a picture inside the hole becomes a quad") {
    const std::vector pictures{
        MenuPicture{.bounds = {.x = 100.0F, .y = -50.0F, .width = 200.0F, .height = 100.0F}},
        MenuPicture{.bounds = {.x = 300.0F, .width = 50.0F, .height = 50.0F}}};
    std::vector<render::QuadDraw> quads;
    AppendBackdropQuads(pictures, kHole, kCamera, quads);

    REQUIRE(quads.size() == 1);
    CheckCorner(quads[0].corners[0], {.x = 0.0F, .y = 99.0F, .z = -99.0F});
    CheckCorner(quads[0].corners[3], {.x = 198.0F, .y = 0.0F, .z = -99.0F});
    CHECK(quads[0].uv.x == doctest::Approx(0.0F));
    CHECK(quads[0].uv.y == doctest::Approx(0.5F));
    CHECK(quads[0].uv.width == doctest::Approx(0.5F));
    CHECK(quads[0].uv.height == doctest::Approx(0.5F));
}

}
