#include "app/hud/minimap.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

#include "sprite_corners.hpp"

namespace rr::app {
namespace {

constexpr render::TextureId kMapImage{5};
constexpr render::TextureId kSheet{6};
constexpr UiImage kMarkers{.texture = kSheet, .widthPx = 32, .heightPx = 32};
constexpr std::size_t kImageSprites = 2;
constexpr std::size_t kArrowSprites = 2;
constexpr std::size_t kBorderSprites = 4;

TrackMap SquareMap(bool isMirrored = false) {
    return {.minX = -512.0F,
            .maxY = 512.0F,
            .maxX = 512.0F,
            .minY = -512.0F,
            .image = kMapImage,
            .isMirrored = isMirrored};
}

MapView PlayerAt(const Vector3& player, std::span<const MapMarker> markers = {}) {
    return {.player = player,
            .playerForward = {.x = 1.0F},
            .viewDirection = {.y = 1.0F},
            .markers = markers};
}

bool HasBounds(const render::SpriteDraw& sprite, const render::Rect& bounds) {
    return sprite.bounds.x == bounds.x && sprite.bounds.y == bounds.y &&
           sprite.bounds.width == bounds.width && sprite.bounds.height == bounds.height;
}

bool IsWithin(const render::SpriteDraw& sprite, const render::Rect& area) {
    return std::ranges::all_of(test::CornersOf(sprite), [&](const render::SpriteCorner& corner) {
        return corner.x >= area.x - 0.001F && corner.x <= area.x + area.width + 0.001F &&
               corner.y >= area.y - 0.001F && corner.y <= area.y + area.height + 0.001F;
    });
}

}

TEST_CASE("the fixed map image fills a 128 square in the bottom right") {
    std::vector<render::SpriteDraw> out;

    DrawFixedMap(SquareMap(), PlayerAt({}), kMarkers, out);

    REQUIRE(out.size() == kImageSprites + kArrowSprites);
    const render::SpriteDraw& upper = out[0];
    CHECK(upper.texture == kMapImage);
    CHECK(test::CornersOf(upper)[0].x == doctest::Approx(497.0F));
    CHECK(test::CornersOf(upper)[0].y == doctest::Approx(337.0F));
    CHECK(test::CornersOf(upper)[0].v == doctest::Approx(0.999F));
    CHECK(test::CornersOf(upper)[1].x == doctest::Approx(625.0F));
    CHECK(test::CornersOf(upper)[1].u == doctest::Approx(0.999F));
    CHECK(test::CornersOf(upper)[2].y == doctest::Approx(465.0F));
    CHECK(test::CornersOf(upper)[2].v == doctest::Approx(0.0F));
    CHECK(test::CornersOf(upper)[0].tint.a == doctest::Approx(200.0F / 255.0F));
}

TEST_CASE("a mirrored fixed map flips its image") {
    std::vector<render::SpriteDraw> out;

    DrawFixedMap(SquareMap(true), PlayerAt({}), kMarkers, out);

    CHECK(test::CornersOf(out[0])[0].v == doctest::Approx(0.0F));
    CHECK(test::CornersOf(out[0])[2].v == doctest::Approx(0.999F));
}

TEST_CASE("fixed map markers sit on their racers and the champion has its own cell") {
    const std::array markers{
        MapMarker{.position = {.x = 512.0F, .y = -512.0F}},
        MapMarker{.position = {.x = -512.0F, .y = 512.0F}, .isChampion = true}};
    std::vector<render::SpriteDraw> out;

    DrawFixedMap(SquareMap(), PlayerAt({}, markers), kMarkers, out);

    REQUIRE(out.size() == kImageSprites + markers.size() + kArrowSprites);
    const render::SpriteDraw& corner = out[kImageSprites];
    CHECK(corner.texture == kSheet);
    CHECK(corner.bounds.x == 617.0F);
    CHECK(corner.bounds.y == 457.0F);
    CHECK(corner.bounds.width == 16.0F);
    CHECK(corner.uv.x == 0.0F);
    CHECK(corner.uv.width == 0.5F);
    const render::SpriteDraw& champion = out[kImageSprites + 1];
    CHECK(champion.bounds.x == 489.0F);
    CHECK(champion.bounds.y == 329.0F);
    CHECK(champion.uv.x == 0.5F);
}

TEST_CASE("a mirrored fixed map measures markers from the mirrored far edge") {
    const std::array markers{MapMarker{.position = {.x = 512.0F, .y = -512.0F}}};
    std::vector<render::SpriteDraw> out;

    DrawFixedMap(SquareMap(true), PlayerAt({}, markers), kMarkers, out);

    CHECK(out[kImageSprites].bounds.y == 457.0F);
}

TEST_CASE("the fixed map arrow points along the player's heading, black under green") {
    std::vector<render::SpriteDraw> out;

    DrawFixedMap(SquareMap(), PlayerAt({.x = 8.0F, .y = 8.0F}), kMarkers, out);

    const render::SpriteDraw& black = out[out.size() - 2];
    const render::SpriteDraw& green = out.back();
    CHECK(test::CornersOf(black)[1].x == doctest::Approx(562.0F + 9.5F));
    CHECK(test::CornersOf(black)[1].y == doctest::Approx(400.0F));
    CHECK(test::CornersOf(black)[1].tint.g == 0.0F);
    CHECK(test::CornersOf(green)[1].x == doctest::Approx(562.0F + 8.0F));
    CHECK(test::CornersOf(green)[1].tint.g == 1.0F);
}

TEST_CASE("the rotating map turns the view direction up") {
    const std::array markers{MapMarker{.position = {.x = 100.0F}}};
    MapView view = PlayerAt({}, markers);
    view.viewDirection = {.x = 1.0F};
    std::vector<render::SpriteDraw> out;

    DrawRotatingMap(SquareMap(), view, kMarkers, out);

    const auto marker =
        std::ranges::find_if(out, [](const render::SpriteDraw& s) { return s.texture == kSheet; });
    REQUIRE(marker != out.end());
    CHECK(marker->bounds.x == 553.0F);
    CHECK(marker->bounds.y == 381.0F);
}

TEST_CASE("the rotating map clips its image to the square and draws a border last") {
    const render::Rect square{.x = 497.0F, .y = 337.0F, .width = 128.0F, .height = 128.0F};
    std::vector<render::SpriteDraw> out;

    DrawRotatingMap(SquareMap(), PlayerAt({.x = 400.0F, .y = 400.0F}), kMarkers, out);

    REQUIRE(out.size() > kArrowSprites + kBorderSprites);
    CHECK(std::ranges::all_of(out, [&](const render::SpriteDraw& sprite) {
        return sprite.texture != kMapImage || IsWithin(sprite, square);
    }));
    CHECK(HasBounds(out[out.size() - kBorderSprites],
                    {.x = 497.0F, .y = 337.0F, .width = 128.0F, .height = 1.0F}));
    CHECK(out.back().bounds.x == 624.0F);
}

TEST_CASE("the rotating map arrow always points up from the centre") {
    std::vector<render::SpriteDraw> out;

    DrawRotatingMap(SquareMap(), PlayerAt({}), kMarkers, out);

    const render::SpriteDraw& black = out[out.size() - kBorderSprites - 2];
    CHECK(test::CornersOf(black)[1].x == doctest::Approx(561.0F));
    CHECK(test::CornersOf(black)[1].y == doctest::Approx(401.0F - 9.5F));
}

}
