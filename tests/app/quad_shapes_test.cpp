#include "app/quad_shapes.hpp"

#include <doctest/doctest.h>

#include <array>
#include <numbers>
#include <vector>

#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr QuadLook kLook{.texture = render::TextureId{7}, .tint = {.r = 0.5F}};

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x));
    CHECK(actual.y == doctest::Approx(expected.y));
    CHECK(actual.z == doctest::Approx(expected.z));
}

TEST_CASE("a billboard spans the camera's right and up axes") {
    std::vector<render::QuadDraw> quads;

    AppendBillboard(kLook, {.z = -10.0F}, 4.0F, 2.0F, {}, quads);

    REQUIRE(quads.size() == 1);
    CHECK(quads[0].texture == kLook.texture);
    CheckNear(quads[0].corners[0], {.x = -2.0F, .y = 1.0F, .z = -10.0F});
    CheckNear(quads[0].corners[3], {.x = 2.0F, .y = -1.0F, .z = -10.0F});
}

TEST_CASE("an upright billboard stays upright when the camera looks down on it") {
    const Transform camera{.rotation = FromAxisAngle({.x = 1.0F}, -std::numbers::pi_v<float> / 4)};
    std::vector<render::QuadDraw> quads;

    AppendUprightBillboard(kLook, {.z = -10.0F}, {.y = 1.0F}, 4.0F, camera, quads);

    REQUIRE(quads.size() == 1);
    CheckNear(quads[0].corners[0], {.x = -2.0F, .y = 1.0F, .z = -10.0F});
    CheckNear(quads[0].corners[3], {.x = 2.0F, .y = -1.0F, .z = -10.0F});
}

TEST_CASE("an upright billboard turns about its axis to face against the view") {
    const Transform camera{.rotation = FromAxisAngle({.y = 1.0F}, std::numbers::pi_v<float> / 2)};
    std::vector<render::QuadDraw> quads;

    AppendUprightBillboard(kLook, {}, {.y = 1.0F}, 4.0F, camera, quads);

    REQUIRE(quads.size() == 1);
    CheckNear(quads[0].corners[0], {.y = 1.0F, .z = 2.0F});
    CheckNear(quads[0].corners[3], {.y = -1.0F, .z = -2.0F});
}

TEST_CASE("an upright billboard seen along its axis takes the camera's right") {
    const Transform camera{.rotation = FromAxisAngle({.x = 1.0F}, -std::numbers::pi_v<float> / 2)};
    std::vector<render::QuadDraw> quads;

    AppendUprightBillboard(kLook, {}, {.y = 1.0F}, 4.0F, camera, quads);

    REQUIRE(quads.size() == 1);
    CheckNear(quads[0].corners[0], {.x = -2.0F, .y = 1.0F});
}

TEST_CASE("a decal lies level, just above its centre") {
    std::vector<render::QuadDraw> quads;

    AppendDecal(kLook, {.y = 5.0F}, 10.0F, quads);

    REQUIRE(quads.size() == 1);
    for (const Vector3& corner : quads[0].corners) {
        CHECK(corner.y == doctest::Approx(5.1F));
    }
    CheckNear(quads[0].corners[0], {.x = -5.0F, .y = 5.1F, .z = -5.0F});
    CheckNear(quads[0].corners[3], {.x = 5.0F, .y = 5.1F, .z = 5.0F});
}

TEST_CASE(
    "a ribbon draws one quad per segment, as wide as asked, facing the camera and tinted per "
    "point") {
    const std::array<RibbonPoint, 3> points{
        RibbonPoint{}, RibbonPoint{.position = {.x = 10.0F}, .tint = {.g = 0.5F}},
        RibbonPoint{.position = {.x = 20.0F}}};
    const Transform camera{.position = {.x = 10.0F, .z = 50.0F}};
    std::vector<render::QuadDraw> quads;

    AppendRibbon(kLook, points, 2.0F, camera, quads);

    REQUIRE(quads.size() == 2);
    CheckNear(quads[0].corners[0], {.y = 1.0F});
    CheckNear(quads[0].corners[3], {.x = 10.0F, .y = -1.0F});
    CheckNear(quads[1].corners[1], {.x = 20.0F, .y = 1.0F});
    CHECK(quads[0].cornerTints[0].g == doctest::Approx(1.0F));
    CHECK(quads[0].cornerTints[3].g == doctest::Approx(0.5F));
    CHECK(quads[1].cornerTints[0].g == doctest::Approx(0.5F));
}

TEST_CASE("an opaque material blends by its texture's alpha as a quad") {
    const QuadLook look =
        LookOf({.texture = render::TextureId{3}, .blend = render::BlendMode::Opaque});

    CHECK(look.texture == render::TextureId{3});
    CHECK(look.blend == render::BlendMode::Alpha);
    CHECK(LookOf({.blend = render::BlendMode::Additive}).blend == render::BlendMode::Additive);
}

TEST_CASE("a material's filter and address reach the quads drawn with it") {
    const QuadLook look =
        LookOf({.filter = render::TextureFilter::Point, .address = render::TextureAddress::Repeat});
    std::vector<render::QuadDraw> quads;

    AppendBillboard(look, {}, 1.0F, 1.0F, {}, quads);
    AppendDecal(look, {}, 1.0F, quads);

    REQUIRE(quads.size() == 2);
    for (const render::QuadDraw& quad : quads) {
        CHECK(quad.sampling.filter == render::TextureFilter::Point);
        CHECK(quad.sampling.address == render::TextureAddress::Repeat);
    }
}

}
}
