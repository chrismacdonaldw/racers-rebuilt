#include "app/ground_decal.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "app/track_space.hpp"
#include "decal_ground.hpp"

namespace rr::app {
namespace {

using CornerUvs = std::array<render::TexturePoint, 4>;

constexpr QuadLook kLook{.texture = render::TextureId{7}};
constexpr GroundDecal kDecal{.centre = {.x = 10.0F, .y = 5.0F, .z = 3.0F},
                             .forward = {.x = 1.0F},
                             .length = 9.0F,
                             .width = 8.0F};
constexpr float kSpan = 1.0F / 0.99F;
constexpr float kTolerance = 0.01F;
constexpr CornerUvs kNoUvs{{{.u = -1.0F, .v = -1.0F},
                            {.u = -1.0F, .v = -1.0F},
                            {.u = -1.0F, .v = -1.0F},
                            {.u = -1.0F, .v = -1.0F}}};

std::vector<render::QuadDraw> Cast(const DecalSurface& surface, const GroundDecal& decal) {
    GroundDecals ground(surface);
    std::vector<render::QuadDraw> quads;
    ground.Append(kLook, decal, quads);
    return quads;
}

DecalSurface Surface(const DecalTriangle& triangle) {
    return DecalSurface(std::vector{triangle});
}

bool IsNear(float actual, float expected) {
    return std::abs(actual - expected) <= kTolerance;
}

bool SpansDecal(const Vector3& game, const render::TexturePoint& uv) {
    const bool isLeft = game.y > kDecal.centre.y;
    const bool isAhead = game.x > kDecal.centre.x;
    return IsNear(game.z, 0.1F) && IsNear(std::abs(game.x - kDecal.centre.x), 4.5F * kSpan) &&
           IsNear(std::abs(game.y - kDecal.centre.y), 4.0F * kSpan) &&
           IsNear(uv.u, isLeft ? 0.99F : 0.0F) && IsNear(uv.v, isAhead ? 0.99F : 0.0F);
}

bool StaysRightOfCentre(const render::QuadDraw& quad) {
    const CornerUvs uvs = quad.cornerUvs.value_or(kNoUvs);
    for (std::size_t corner = 0; corner < uvs.size(); ++corner) {
        if (ToGame(quad.corners.at(corner)).y > kDecal.centre.y + kTolerance ||
            uvs.at(corner).u > 0.5F + kTolerance || uvs.at(corner).u < 0.0F) {
            return false;
        }
    }
    return true;
}

bool StaysOnWallInSquare(const Vector3& corner) {
    const Vector3 game = ToGame(corner);
    return game.x > 8.8F && game.x < 13.0F &&
           std::abs(game.y - kDecal.centre.y) <= (4.0F * kSpan) + kTolerance;
}

TEST_CASE("a decal on flat ground spans its size, with the original's 0.99 texture margin") {
    const std::vector<render::QuadDraw> quads = Cast(test::FlatDecalGround(), kDecal);

    REQUIRE(quads.size() == 1);
    CHECK(quads[0].texture == render::TextureId{7});
    const CornerUvs uvs = quads[0].cornerUvs.value_or(kNoUvs);
    for (std::size_t corner = 0; corner < uvs.size(); ++corner) {
        CHECK(SpansDecal(ToGame(quads[0].corners.at(corner)), uvs.at(corner)));
    }
}

TEST_CASE("a decal over a triangle's edge keeps only the part on the triangle") {
    const DecalSurface half(
        std::vector{test::GameTriangle({.x = -100.0F, .y = -100.0F}, {.x = 100.0F, .y = -100.0F},
                                       {.x = 100.0F, .y = 5.0F}),
                    test::GameTriangle({.x = -100.0F, .y = -100.0F}, {.x = 100.0F, .y = 5.0F},
                                       {.x = -100.0F, .y = 5.0F})});

    const std::vector<render::QuadDraw> quads = Cast(half, kDecal);

    REQUIRE_FALSE(quads.empty());
    CHECK(std::ranges::all_of(quads, StaysRightOfCentre));
}

TEST_CASE("a decal lies on a near-vertical slope without leaving its ground square") {
    const DecalSurface wall = Surface(test::GameTriangle({.x = 9.0F, .y = -100.0F, .z = -10.0F},
                                                         {.x = 13.0F, .y = -100.0F, .z = 30.0F},
                                                         {.x = 9.0F, .y = 300.0F, .z = -10.0F}));

    const std::vector<render::QuadDraw> quads = Cast(wall, kDecal);

    REQUIRE(quads.size() == 1);
    CHECK(std::ranges::all_of(quads[0].corners, StaysOnWallInSquare));
}

TEST_CASE("triangles facing away or beside the decal take none of it") {
    const DecalTriangle facingDown = test::GameTriangle(
        {.x = -100.0F, .y = -100.0F}, {.x = -100.0F, .y = 300.0F}, {.x = 300.0F, .y = -100.0F});
    const DecalTriangle beside = test::GameTriangle(
        {.x = 100.0F, .y = 100.0F}, {.x = 200.0F, .y = 100.0F}, {.x = 100.0F, .y = 200.0F});

    CHECK(Cast(Surface(facingDown), kDecal).empty());
    CHECK(Cast(Surface(beside), kDecal).empty());
}

TEST_CASE("a decal reaches from 6 above its centre to 15 below that") {
    CHECK(Cast(test::FlatDecalGround(-6.5F), kDecal).empty());
    CHECK(Cast(test::FlatDecalGround(-5.5F), kDecal).size() == 1);
    CHECK(Cast(test::FlatDecalGround(9.5F), kDecal).empty());
}

}
}
