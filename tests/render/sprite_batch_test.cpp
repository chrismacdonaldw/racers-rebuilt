#include "render/sprite_batch.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rr::render {
namespace {

constexpr TextureId kTile{3};
constexpr TextureId kIcon{4};

struct Batched {
    std::vector<SpriteVertex> vertices;
    std::vector<SpriteBatch> batches;
};

Batched Batch(const std::vector<SpriteDraw>& sprites, float edgeShift = 0.0F) {
    Batched out;
    BatchSprites(sprites, edgeShift, out.vertices, out.batches);
    return out;
}

TEST_CASE("a thousand sprites on one texture make one batch") {
    const std::vector<SpriteDraw> sprites(1000, SpriteDraw{.texture = kTile});

    const Batched batched = Batch(sprites);

    REQUIRE(batched.batches.size() == 1);
    CHECK(batched.batches[0].spriteCount == 1000);
    CHECK(batched.vertices.size() == std::size_t{1000} * kVerticesPerSprite);
}

TEST_CASE("a change of texture or blend starts a new batch") {
    const std::vector<SpriteDraw> sprites{
        {.texture = kTile}, {.texture = kTile},
        {.texture = kIcon}, {.texture = kIcon, .blend = BlendMode::Additive},
        {.texture = kTile},
    };

    const Batched batched = Batch(sprites);

    REQUIRE(batched.batches.size() == 4);
    CHECK(batched.batches[0].spriteCount == 2);
    CHECK(batched.batches[1].firstSprite == 2);
    CHECK(batched.batches[2].blend == BlendMode::Additive);
    CHECK(batched.batches[3].texture == kTile);
    CHECK(batched.batches[3].firstSprite == 4);
}

TEST_CASE("corners carry the bounds, texture coordinates and tint") {
    const SpriteDraw sprite{.bounds = {.x = 10.0F, .y = 20.0F, .width = 30.0F, .height = 40.0F},
                            .uv = {.x = 0.5F, .y = 0.25F, .width = 0.5F, .height = 0.25F},
                            .tint = {.r = 0.5F}};

    const Batched batched = Batch({sprite});

    REQUIRE(batched.vertices.size() == kVerticesPerSprite);
    const SpriteVertex& topLeft = batched.vertices[0];
    const SpriteVertex& bottomRight = batched.vertices[3];
    CHECK(topLeft.x == 10.0F);
    CHECK(topLeft.y == 20.0F);
    CHECK(topLeft.u == 0.5F);
    CHECK(topLeft.v == 0.25F);
    CHECK(bottomRight.x == 40.0F);
    CHECK(bottomRight.y == 60.0F);
    CHECK(bottomRight.u == 1.0F);
    CHECK(bottomRight.v == 0.5F);
    CHECK(bottomRight.color.r == 0.5F);
}

TEST_CASE("set corners replace the bounds, texture coordinates and tint") {
    const SpriteDraw sprite{.texture = kTile,
                            .bounds = {.width = 30.0F, .height = 40.0F},
                            .corners = std::array{SpriteCorner{.x = 1.0F, .u = 0.5F},
                                                  SpriteCorner{.x = 2.0F, .tint = {.g = 0.5F}},
                                                  SpriteCorner{.y = 3.0F, .v = 0.25F},
                                                  SpriteCorner{.y = 3.0F, .v = 0.25F}}};

    const Batched batched = Batch({{.texture = kIcon}, sprite});

    REQUIRE(batched.vertices.size() == std::size_t{2} * kVerticesPerSprite);
    REQUIRE(batched.batches.size() == 2);
    CHECK(batched.batches[1].texture == kTile);
    CHECK(batched.batches[1].firstSprite == 1);
    CHECK(batched.vertices[4].x == 1.0F);
    CHECK(batched.vertices[4].u == 0.5F);
    CHECK(batched.vertices[5].color.g == 0.5F);
    CHECK(batched.vertices[6].v == 0.25F);
    CHECK(batched.vertices[7].y == 3.0F);
}

TEST_CASE("anchored sprites move out to the window's sides, and filling ones stretch") {
    const Rect bounds{.x = 10.0F, .width = 20.0F, .height = 5.0F};
    const std::vector<SpriteDraw> sprites{{.bounds = bounds},
                                          {.bounds = bounds, .anchor = SpriteAnchor::Left},
                                          {.bounds = bounds, .anchor = SpriteAnchor::Right},
                                          {.bounds = bounds, .anchor = SpriteAnchor::Fill}};

    const Batched batched = Batch(sprites, 80.0F);

    REQUIRE(batched.vertices.size() == std::size_t{4} * kVerticesPerSprite);
    CHECK(batched.vertices[0].x == 10.0F);
    CHECK(batched.vertices[4].x == -70.0F);
    CHECK(batched.vertices[9].x == 110.0F);
    CHECK(batched.vertices[12].x == -70.0F);
    CHECK(batched.vertices[13].x == 110.0F);
}

TEST_CASE("anchored corners move with their side") {
    const SpriteDraw sprite{.corners = std::array{SpriteCorner{.x = 1.0F}, SpriteCorner{.x = 2.0F},
                                                  SpriteCorner{.x = 1.0F}, SpriteCorner{.x = 2.0F}},
                            .anchor = SpriteAnchor::Fill};

    const Batched batched = Batch({sprite}, 10.0F);

    CHECK(batched.vertices[0].x == -9.0F);
    CHECK(batched.vertices[1].x == 12.0F);
    CHECK(batched.vertices[2].x == -9.0F);
    CHECK(batched.vertices[3].x == 12.0F);
}

TEST_CASE("sprites beside the canvas draw only when the window is wider, unmoved") {
    const std::vector<SpriteDraw> sprites{
        {.texture = kTile},
        {.texture = kIcon,
         .bounds = {.x = -640.0F, .width = 640.0F, .height = 480.0F},
         .anchor = SpriteAnchor::Beside}};

    const Batched narrow = Batch(sprites);
    const Batched wide = Batch(sprites, 100.0F);

    REQUIRE(narrow.batches.size() == 1);
    CHECK(narrow.batches[0].texture == kTile);
    REQUIRE(wide.batches.size() == 2);
    CHECK(wide.batches[1].texture == kIcon);
    CHECK(wide.vertices[kVerticesPerSprite].x == -640.0F);
    CHECK(wide.vertices[kVerticesPerSprite + 1].x == 0.0F);
}

TEST_CASE("sprite indices make two triangles over each sprite's corners") {
    const std::vector<std::uint16_t> indices = SpriteIndices(2);

    CHECK(indices == std::vector<std::uint16_t>{0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7});
}

}
}
