#include "app/racer/car_shadows.hpp"

#include <doctest/doctest.h>

#include <numbers>
#include <vector>

#include "../../sim/physics/plane_world.hpp"
#include "app/track_space.hpp"
#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kAt{.x = 10.0F, .y = 5.0F, .z = 3.0F};

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x));
    CHECK(actual.y == doctest::Approx(expected.y));
    CHECK(actual.z == doctest::Approx(expected.z));
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
                                   const sim::CollisionQuery& ground) {
    std::vector<render::QuadDraw> quads;
    shadows.Append(grid.poses, grid.racers, grid.looks, ground, quads);
    return quads;
}

TEST_CASE("a car's shadow lies on the ground under it, its length along the car") {
    const std::vector<sim::ChassisSpec> chassis(1);
    const CarShadows shadows(Materials(), chassis);

    const std::vector<render::QuadDraw> quads =
        Cast(shadows, OneCar({.position = kAt}), test::FlatGround());

    REQUIRE(quads.size() == 1);
    CHECK(quads[0].texture == render::TextureId{4});
    CheckNear(ToGame(quads[0].corners[0]), {.x = 14.5F, .y = 9.0F, .z = 0.1F});
    CheckNear(ToGame(quads[0].corners[1]), {.x = 14.5F, .y = 1.0F, .z = 0.1F});
    CheckNear(ToGame(quads[0].corners[3]), {.x = 5.5F, .y = 1.0F, .z = 0.1F});
}

TEST_CASE("a shadow turns with its car and shrinks with it") {
    const std::vector<sim::ChassisSpec> chassis(1);
    const CarShadows shadows(Materials(), chassis);
    Grid grid = OneCar({.position = kAt,
                        .rotation = FromAxisAngle({.z = 1.0F}, std::numbers::pi_v<float> / 2.0F)});
    grid.looks[0].scale = 0.5F;

    const std::vector<render::QuadDraw> quads = Cast(shadows, grid, test::FlatGround());

    REQUIRE(quads.size() == 1);
    CheckNear(ToGame(quads[0].corners[0]), {.x = 8.0F, .y = 7.25F, .z = 0.1F});
}

TEST_CASE("a shadow on a slope lies in the slope, as cast straight down") {
    const Vector3 normal{.x = -0.6F, .z = 0.8F};
    const test::PlaneWorld slope({test::TestPlane{.normal = normal}});
    const std::vector<sim::ChassisSpec> chassis(1);
    const CarShadows shadows(Materials(), chassis);

    const std::vector<render::QuadDraw> quads =
        Cast(shadows, OneCar({.position = {.x = 0.0F, .z = 3.0F}}), slope);

    REQUIRE(quads.size() == 1);
    for (const Vector3& corner : quads[0].corners) {
        CHECK(Dot(ToGame(corner), normal) == doctest::Approx(0.1F));
    }
    CheckNear(ToGame(quads[0].corners[0]),
              Vector3{.x = 4.5F, .y = 4.0F, .z = 3.375F} + (normal * 0.1F));
}

TEST_CASE("ghosts, cars high above the ground and slots without a material cast no shadow") {
    const std::vector<sim::ChassisSpec> chassis(3);
    const CarShadows shadows(Materials(), chassis);
    Grid grid{.poses = {{.transform = TransformToEngine({.position = kAt})},
                        {.transform = TransformToEngine({.position = {.z = 10.0F}})},
                        {.transform = TransformToEngine({.position = kAt})}},
              .racers = {{.isGhost = true}, {}, {}},
              .looks = {{}, {}, {}}};

    CHECK(Cast(shadows, grid, test::FlatGround()).empty());
    grid.poses[1].transform = TransformToEngine({.position = {.z = 8.0F}});
    CHECK(Cast(shadows, grid, test::FlatGround()).size() == 1);
}

}
}
