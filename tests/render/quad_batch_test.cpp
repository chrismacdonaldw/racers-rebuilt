#include "render/quad_batch.hpp"

#include <doctest/doctest.h>

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

}
}
