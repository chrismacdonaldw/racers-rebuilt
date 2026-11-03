#include "app/racer/car_shadows.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <numbers>
#include <span>
#include <vector>

#include "../decal_ground.hpp"
#include "app/track_space.hpp"
#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kAt{.x = 10.0F, .y = 5.0F, .z = 3.0F};
constexpr float kSpan = 1.0F / 0.99F;

void CheckSpan(const render::QuadDraw& quad, float halfAlongX, float halfAlongY,
               const Vector3& centre = kAt) {
    for (const Vector3& corner : quad.corners) {
        const Vector3 game = ToGame(corner);
        CHECK(std::abs(game.x - centre.x) ==
              doctest::Approx(halfAlongX * kSpan).epsilon(test::kClipEpsilon));
        CHECK(std::abs(game.y - centre.y) ==
              doctest::Approx(halfAlongY * kSpan).epsilon(test::kClipEpsilon));
        CHECK(game.z == doctest::Approx(0.1F));
    }
}

assets::NameMap<render::Material> Materials() {
    return {{"CARSHAD0", {.texture = render::TextureId{4}}},
            {"CARSHAD1", {.texture = render::TextureId{5}}}};
}

struct Grid {
    std::vector<sim::EntitySnapshot> poses;
    std::vector<sim::RacerSnapshot> racers;
    std::vector<CarLook> looks;
};

Grid OneCar(const Transform& game) {
    return {.poses = {{.transform = TransformToEngine(game)}}, .racers = {{}}, .looks = {{}}};
}

std::vector<render::QuadDraw> Cast(const CarShadows& shadows, const Grid& grid,
                                   const DecalSurface& surface,
                                   std::span<const Vector3> cameras = {}) {
    GroundDecals ground(surface);
    std::vector<render::QuadDraw> quads;
    shadows.Append(grid.poses, grid.racers, grid.looks, cameras, ground, quads);
    return quads;
}

TEST_CASE("a car's shadow lies on the ground under it, its width along the car as the original") {
    const std::vector<ShadowCaster> casters(1);
    const CarShadows shadows(Materials(), casters);

    const std::vector<render::QuadDraw> quads =
        Cast(shadows, OneCar({.position = kAt}), test::FlatDecalGround());

    REQUIRE(quads.size() == 1);
    CHECK(quads[0].texture == render::TextureId{4});
    CheckSpan(quads[0], 4.0F, 4.5F);
}

TEST_CASE("a car's shadow wears its silhouette and centres on its wheels, turned with it") {
    const std::vector<ShadowCaster> casters{
        {.centre = {.x = 1.0F}, .silhouette = render::TextureId{9}}};
    const CarShadows shadows(Materials(), casters);
    const Grid grid =
        OneCar({.position = kAt,
                .rotation = FromAxisAngle({.z = 1.0F}, std::numbers::pi_v<float> / 2.0F)});

    const std::vector<render::QuadDraw> quads = Cast(shadows, grid, test::FlatDecalGround());

    REQUIRE(quads.size() == 1);
    CHECK(quads[0].texture == render::TextureId{9});
    CheckSpan(quads[0], 4.5F, 4.0F, kAt + Vector3{.y = 1.0F});
}

TEST_CASE("a shadow turns with its car and shrinks with it") {
    const std::vector<ShadowCaster> casters(1);
    const CarShadows shadows(Materials(), casters);
    Grid grid = OneCar({.position = kAt,
                        .rotation = FromAxisAngle({.z = 1.0F}, std::numbers::pi_v<float> / 2.0F)});
    grid.looks[0].scale = 0.5F;

    const std::vector<render::QuadDraw> quads = Cast(shadows, grid, test::FlatDecalGround());

    REQUIRE(quads.size() == 1);
    CheckSpan(quads[0], 2.25F, 2.0F);
}

TEST_CASE("a shadow on a slope lies in the slope, as cast straight down") {
    const Vector3 normal{.x = -0.6F, .z = 0.8F};
    const DecalSurface slope(std::vector{test::GameTriangle(
        {.x = -100.0F, .y = -100.0F, .z = -75.0F}, {.x = 300.0F, .y = -100.0F, .z = 225.0F},
        {.x = -100.0F, .y = 300.0F, .z = -75.0F})});
    const std::vector<ShadowCaster> casters(1);
    const CarShadows shadows(Materials(), casters);

    const std::vector<render::QuadDraw> quads =
        Cast(shadows, OneCar({.position = {.x = 0.0F, .z = 3.0F}}), slope);

    REQUIRE(quads.size() == 1);
    for (const Vector3& corner : quads[0].corners) {
        const Vector3 game = ToGame(corner);
        CHECK(Dot(game, normal) == doctest::Approx(0.1F));
        CHECK(std::abs(game.y) == doctest::Approx(4.5F * kSpan).epsilon(test::kClipEpsilon));
    }
}

TEST_CASE("ghosts, cars high above the ground and slots without a material cast no shadow") {
    const std::vector<ShadowCaster> casters(3);
    const CarShadows shadows(Materials(), casters);
    Grid grid{.poses = {{.transform = TransformToEngine({.position = kAt})},
                        {.transform = TransformToEngine({.position = {.z = 10.0F}})},
                        {.transform = TransformToEngine({.position = kAt})}},
              .racers = {{.isGhost = true}, {}, {}},
              .looks = {{}, {}, {}}};

    CHECK(Cast(shadows, grid, test::FlatDecalGround()).empty());
    grid.poses[1].transform = TransformToEngine({.position = {.z = 8.0F}});
    CHECK(Cast(shadows, grid, test::FlatDecalGround()).size() == 1);
}

TEST_CASE("the original's shadow alpha fades from 50 to 100 units") {
    CHECK(ShadowAlpha(2500.0F) == 191);
    CHECK(ShadowAlpha(5625.0F) == 112);
    CHECK(ShadowAlpha(9999.0F) == 1);
    CHECK(ShadowAlpha(10000.0F) == 0);
}

TEST_CASE("a car's shadow fades by its player's camera, the first serving the rest") {
    const std::vector<ShadowCaster> casters(3);
    assets::NameMap<render::Material> materials = Materials();
    materials.emplace("CARSHAD2", render::Material{.texture = render::TextureId{6}});
    const CarShadows shadows(materials, casters);
    const Grid grid{.poses = {{.transform = TransformToEngine({.position = kAt})},
                              {.transform = TransformToEngine({.position = kAt})},
                              {.transform = TransformToEngine({.position = kAt})}},
                    .racers = {{}, {}, {}},
                    .looks = {{}, {}, {}}};
    const std::vector<Vector3> cameras{ToEngine(kAt + Vector3{.x = 40.0F}),
                                       ToEngine(kAt + Vector3{.y = 100.0F})};

    const std::vector<render::QuadDraw> quads =
        Cast(shadows, grid, test::FlatDecalGround(), cameras);

    REQUIRE(quads.size() == 2);
    CHECK(quads[0].texture == render::TextureId{4});
    CHECK(quads[1].texture == render::TextureId{6});
    CHECK(quads[0].tint.a == doctest::Approx(191.0F / 255.0F));
}

}
}
