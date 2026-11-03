#include "render/quad_batch.hpp"

#include <doctest/doctest.h>

#include <array>
#include <vector>

namespace rr::render {
namespace {

constexpr TextureId kSmoke{5};
constexpr TextureId kSpark{6};

TEST_CASE("quads batch like sprites and keep their corners in order") {
    const std::vector<QuadDraw> quads{
        {.texture = kSmoke,
         .corners = {Vector3{.x = 1.0F}, Vector3{.x = 2.0F}, Vector3{.x = 3.0F},
                     Vector3{.x = 4.0F}},
         .uv = {.x = 0.25F, .y = 0.5F, .width = 0.5F, .height = 0.25F},
         .tint = {.g = 0.5F}},
        {.texture = kSmoke},
        {.texture = kSpark, .blend = BlendMode::Additive},
    };
    std::vector<QuadVertex> vertices;
    std::vector<SpriteBatch> batches;

    BatchQuads(quads, vertices, batches);

    REQUIRE(vertices.size() == 3 * kVerticesPerSprite);
    CHECK(vertices[0].position.x == doctest::Approx(1.0F));
    CHECK(vertices[3].position.x == doctest::Approx(4.0F));
    CHECK(vertices[0].u == doctest::Approx(0.25F));
    CHECK(vertices[3].u == doctest::Approx(0.75F));
    CHECK(vertices[3].v == doctest::Approx(0.75F));
    CHECK(vertices[2].color.g == doctest::Approx(0.5F));
    REQUIRE(batches.size() == 2);
    CHECK(batches[0].spriteCount == 2);
    CHECK(batches[1].firstSprite == 2);
    CHECK(batches[1].blend == BlendMode::Additive);
}

TEST_CASE("corner uvs replace a quad's uv rectangle") {
    const std::vector<QuadDraw> quads{
        {.texture = kSmoke,
         .uv = {.x = 0.25F, .y = 0.5F, .width = 0.5F, .height = 0.25F},
         .cornerUvs = std::array<TexturePoint, 4>{{{.u = 0.1F, .v = 0.2F},
                                                   {.u = 0.3F, .v = 0.4F},
                                                   {.u = 0.5F, .v = 0.6F},
                                                   {.u = 0.7F, .v = 0.8F}}}}};
    std::vector<QuadVertex> vertices;
    std::vector<SpriteBatch> batches;

    BatchQuads(quads, vertices, batches);

    REQUIRE(vertices.size() == kVerticesPerSprite);
    CHECK(vertices[0].u == doctest::Approx(0.1F));
    CHECK(vertices[2].v == doctest::Approx(0.6F));
    CHECK(vertices[3].u == doctest::Approx(0.7F));
}

TEST_CASE("quads sampled differently start a new batch") {
    constexpr TextureSampling kBlocky{.filter = TextureFilter::Point,
                                      .address = TextureAddress::Repeat};
    const std::vector<QuadDraw> quads{
        {.texture = kSmoke}, {.texture = kSmoke, .sampling = kBlocky}, {.texture = kSmoke}};
    std::vector<QuadVertex> vertices;
    std::vector<SpriteBatch> batches;

    BatchQuads(quads, vertices, batches);

    REQUIRE(batches.size() == 3);
    CHECK(batches[0].sampling == TextureSampling{});
    CHECK(batches[1].sampling == kBlocky);
    CHECK(batches[2].firstSprite == 2);
}

TEST_CASE("a quad's corner tints scale its tint at each corner") {
    const std::vector<QuadDraw> quads{
        {.tint = {.r = 0.5F},
         .cornerTints = {Color{}, Color{.r = 0.5F}, Color{}, Color{.b = 0.0F}}}};
    std::vector<QuadVertex> vertices;
    std::vector<SpriteBatch> batches;

    BatchQuads(quads, vertices, batches);

    REQUIRE(vertices.size() == kVerticesPerSprite);
    CHECK(vertices[0].color.r == doctest::Approx(0.5F));
    CHECK(vertices[1].color.r == doctest::Approx(0.25F));
    CHECK(vertices[3].color.b == doctest::Approx(0.0F));
    CHECK(vertices[3].color.r == doctest::Approx(0.5F));
}

}
}
