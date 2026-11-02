#include "app/hud/hud_shapes.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

#include "sprite_corners.hpp"

namespace rr::app {
namespace {

constexpr render::TextureId kTexture{7};
constexpr render::Rect kClip{.x = 0.0F, .y = 0.0F, .width = 10.0F, .height = 10.0F};
constexpr render::Color kRed{.r = 1.0F, .g = 0.0F, .b = 0.0F};
constexpr render::Color kBlue{.r = 0.0F, .g = 0.0F, .b = 1.0F};

render::SpriteCorner At(float x, float y, render::Color tint = {}) {
    return {.x = x, .y = y, .u = x / 10.0F, .v = y / 10.0F, .tint = tint};
}

bool IsClippedInPlace(const render::SpriteCorner& corner) {
    return corner.x >= kClip.x && corner.x <= kClip.x + kClip.width && corner.y >= kClip.y &&
           corner.y <= kClip.y + kClip.height && corner.u == doctest::Approx(corner.x / 10.0F) &&
           corner.v == doctest::Approx(corner.y / 10.0F);
}

}

TEST_CASE("a triangle repeats its last corner as a sprite") {
    std::vector<render::SpriteDraw> out;

    AppendTriangle({At(1.0F, 2.0F), At(3.0F, 4.0F), At(5.0F, 6.0F)}, kTexture, out);

    REQUIRE(out.size() == 1);
    CHECK(out[0].texture == kTexture);
    CHECK(test::CornersOf(out[0])[0].x == 1.0F);
    CHECK(test::CornersOf(out[0])[2].y == 6.0F);
    CHECK(test::CornersOf(out[0])[3].x == test::CornersOf(out[0])[2].x);
}

TEST_CASE("a triangle inside the clip passes through whole") {
    std::vector<render::SpriteDraw> out;

    AppendClipped({At(1.0F, 1.0F), At(9.0F, 1.0F), At(1.0F, 9.0F)}, kClip, kTexture, out);

    REQUIRE(out.size() == 1);
    CHECK(test::CornersOf(out[0])[1].x == 9.0F);
    CHECK(test::CornersOf(out[0])[2].y == 9.0F);
}

TEST_CASE("a triangle outside the clip draws nothing") {
    std::vector<render::SpriteDraw> out;

    AppendClipped({At(11.0F, 1.0F), At(19.0F, 1.0F), At(11.0F, 9.0F)}, kClip, kTexture, out);

    CHECK(out.empty());
}

TEST_CASE("a triangle across the clip edge is cut to the clip") {
    std::vector<render::SpriteDraw> out;

    AppendClipped({At(5.0F, 2.0F, kRed), At(15.0F, 2.0F, kBlue), At(5.0F, 8.0F, kRed)}, kClip,
                  kTexture, out);

    REQUIRE(out.size() == 2);
    CHECK(std::ranges::all_of(out, [](const render::SpriteDraw& sprite) {
        return std::ranges::all_of(test::CornersOf(sprite), IsClippedInPlace);
    }));
    CHECK(test::CornersOf(out[0])[1].x == doctest::Approx(10.0F));
    CHECK(test::CornersOf(out[0])[1].tint.r == 0.0F);
}

}
