#include "app/menu/side_tiles.hpp"

#include <doctest/doctest.h>

#include <vector>

namespace rr::app {
namespace {

constexpr render::TextureId kBackdrop{5};
constexpr render::Rect kMirrored{.x = 1.0F, .width = -1.0F, .height = 1.0F};
constexpr render::Rect kPlain{.width = 1.0F, .height = 1.0F};

std::vector<render::SpriteDraw> TilesOf(const render::Rect& bounds) {
    std::vector<render::SpriteDraw> tiles;
    AppendSideTiles({.texture = kBackdrop, .bounds = bounds, .tint = {.g = 0.5F}}, tiles);
    return tiles;
}

void CheckLook(const render::SpriteDraw& tile) {
    CHECK(tile.bounds.height == 480.0F);
    CHECK(tile.texture == kBackdrop);
    CHECK(tile.tint.g == 0.5F);
    CHECK(tile.anchor == render::SpriteAnchor::Beside);
}

void CheckTile(const render::SpriteDraw& tile, float x, const render::Rect& uv) {
    CHECK(tile.bounds.x == x);
    CHECK(tile.uv.x == uv.x);
    CHECK(tile.uv.width == uv.width);
    CheckLook(tile);
}

}

TEST_CASE("a full-canvas backdrop tiles out both sides, mirrored first") {
    const std::vector<render::SpriteDraw> tiles = TilesOf({.width = 640.0F, .height = 480.0F});

    REQUIRE(tiles.size() == 4);
    CheckTile(tiles[0], -640.0F, kMirrored);
    CheckTile(tiles[1], -1280.0F, kPlain);
    CheckTile(tiles[2], 640.0F, kMirrored);
    CheckTile(tiles[3], 1280.0F, kPlain);
}

TEST_CASE("a full-height half tiles out only from the side it touches") {
    const std::vector<render::SpriteDraw> tiles =
        TilesOf({.x = 320.0F, .width = 320.0F, .height = 480.0F});

    REQUIRE(tiles.size() == 2);
    CHECK(tiles[0].bounds.x == 640.0F);
    CHECK(tiles[1].bounds.x == 960.0F);
}

TEST_CASE("pictures short of the canvas's height or away from its sides get no tiles") {
    CHECK(TilesOf({.width = 640.0F, .height = 400.0F}).empty());
    CHECK(TilesOf({.x = 100.0F, .width = 400.0F, .height = 480.0F}).empty());
}

}
