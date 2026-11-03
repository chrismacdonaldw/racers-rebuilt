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

void CheckSpan(const render::QuadDraw& quad, float halfAlongX, float halfAlongY) {
    for (const Vector3& corner : quad.corners) {
        const Vector3 game = ToGame(corner);
        CHECK(std::abs(game.x - kAt.x) ==
              doctest::Approx(halfAlongX * kSpan).epsilon(test::kClipEpsilon));
        CHECK(std::abs(game.y - kAt.y) ==
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

TEST_CASE("a car's shadow lies on the ground under it, its length along the car") {
    const std::vector<sim::ChassisSpec> chassis(1);
    const CarShadows shadows(Materials(), chassis);

    const std::vector<render::QuadDraw> quads =
        Cast(shadows, OneCar({.position = kAt}), test::FlatDecalGround());

    REQUIRE(quads.size() == 1);
    CHECK(quads[0].texture == render::TextureId{4});
    CheckSpan(quads[0], 4.5F, 4.0F);
}

TEST_CASE("a shadow turns with its car and shrinks with it") {
    const std::vector<sim::ChassisSpec> chassis(1);
    const CarShadows shadows(Materials(), chassis);
    Grid grid = OneCar({.position = kAt,
                        .rotation = FromAxisAngle({.z = 1.0F}, std::numbers::pi_v<float> / 2.0F)});
    grid.looks[0].scale = 0.5F;

    const std::vector<render::QuadDraw> quads = Cast(shadows, grid, test::FlatDecalGround());

    REQUIRE(quads.size() == 1);
    CheckSpan(quads[0], 2.0F, 2.25F);
}

TEST_CASE("a shadow on a slope lies in the slope, as cast straight down") {
    const Vector3 normal{.x = -0.6F, .z = 0.8F};
    const DecalSurface slope(std::vector{test::GameTriangle(
        {.x = -100.0F, .y = -100.0F, .z = -75.0F}, {.x = 300.0F, .y = -100.0F, .z = 225.0F},
        {.x = -100.0F, .y = 300.0F, .z = -75.0F})});
    const std::vector<sim::ChassisSpec> chassis(1);
    const CarShadows shadows(Materials(), chassis);

    const std::vector<render::QuadDraw> quads =
        Cast(shadows, OneCar({.position = {.x = 0.0F, .z = 3.0F}}), slope);

    REQUIRE(quads.size() == 1);
    for (const Vector3& corner : quads[0].corners) {
        const Vector3 game = ToGame(corner);
        CHECK(Dot(game, normal) == doctest::Approx(0.1F));
        CHECK(std::abs(game.y) == doctest::Approx(4.0F * kSpan).epsilon(test::kClipEpsilon));
    }
}

TEST_CASE("ghosts, cars high above the ground and slots without a material cast no shadow") {
    const std::vector<sim::ChassisSpec> chassis(3);
    const CarShadows shadows(Materials(), chassis);
    Grid grid{.poses = {{.transform = TransformToEngine({.position = kAt})},
                        {.transform = TransformToEngine({.position = {.z = 10.0F}})},
                        {.transform = TransformToEngine({.position = kAt})}},
              .racers = {{.isGhost = true}, {}, {}},
              .looks = {{}, {}, {}}};

    CHECK(Cast(shadows, grid, test::FlatDecalGround()).empty());
    grid.poses[1].transform = TransformToEngine({.position = {.z = 8.0F}});
    CHECK(Cast(shadows, grid, test::FlatDecalGround()).size() == 1);
}

TEST_CASE("a car casts its shadow only within 500 of its player's camera") {
    const std::vector<sim::ChassisSpec> chassis(3);
    assets::NameMap<render::Material> materials = Materials();
    materials.emplace("CARSHAD2", render::Material{.texture = render::TextureId{6}});
    const CarShadows shadows(materials, chassis);
    const Grid grid{.poses = {{.transform = TransformToEngine({.position = kAt})},
                              {.transform = TransformToEngine({.position = kAt})},
                              {.transform = TransformToEngine({.position = kAt})}},
                    .racers = {{}, {}, {}},
                    .looks = {{}, {}, {}}};
    const std::vector<Vector3> cameras{ToEngine(kAt + Vector3{.x = 499.0F}),
                                       ToEngine(kAt + Vector3{.y = 500.0F})};

    const std::vector<render::QuadDraw> quads =
        Cast(shadows, grid, test::FlatDecalGround(), cameras);

    REQUIRE(quads.size() == 2);
    CHECK(quads[0].texture == render::TextureId{4});
    CHECK(quads[1].texture == render::TextureId{6});
}

}
}
