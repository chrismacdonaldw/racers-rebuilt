#include "app/racer/racer_light.hpp"

#include <doctest/doctest.h>

#include <array>
#include <numbers>

namespace rr::app {
namespace {

constexpr std::uint8_t kLitGrey = 150;
constexpr std::uint8_t kAmbientGrey = 50;
constexpr Vector3 kTowardLight{.x = 1.0F, .y = 0.0F, .z = 1.0F};

assets::GeometryAsset Triangle() {
    return {.vertices = {{.normal = kTowardLight},
                         {.normal = -kTowardLight},
                         {.normal = -kTowardLight, .joint = 1}},
            .indices = {0, 1, 2},
            .materials = {"grey"},
            .ranges = {{.firstIndex = 0, .indexCount = 3}}};
}

assets::NameMap<assets::MaterialAsset> GreyMaterials() {
    assets::NameMap<assets::MaterialAsset> materials;
    materials["GREY"] = {.ambient = {100, 100, 100, 255}, .diffuse = {100, 100, 100, 255}};
    return materials;
}

}

TEST_CASE("built racer light adds the white light to half the material's ambient") {
    assets::GeometryAsset geometry = Triangle();

    BakeBuiltRacerLight(geometry, GreyMaterials(), {});

    CHECK(geometry.vertices[0].rgba == Rgba8{kLitGrey, kLitGrey, kLitGrey, 255});
    CHECK(geometry.vertices[1].rgba == Rgba8{kAmbientGrey, kAmbientGrey, kAmbientGrey, 255});
    CHECK(geometry.vertices[2].rgba == Rgba8{kAmbientGrey, kAmbientGrey, kAmbientGrey, 255});
    CHECK(geometry.vertices[0].normal == Vector3{});
}

TEST_CASE("built racer light turns each jointed normal by its joint") {
    assets::GeometryAsset geometry = Triangle();
    const std::array joints{
        Transform{}, Transform{.rotation = FromAxisAngle({.y = 1.0F}, std::numbers::pi_v<float>)}};

    BakeBuiltRacerLight(geometry, GreyMaterials(), joints);

    CHECK(geometry.vertices[1].rgba[0] == kAmbientGrey);
    CHECK(geometry.vertices[2].rgba[0] == kLitGrey);
}

TEST_CASE("built racer light leaves a vertex of an unknown material its color") {
    assets::GeometryAsset geometry = Triangle();
    geometry.vertices[0].rgba = {1, 2, 3, 4};

    BakeBuiltRacerLight(geometry, {}, {});

    CHECK(geometry.vertices[0].rgba == Rgba8{1, 2, 3, 4});
    CHECK(geometry.vertices[0].normal == Vector3{});
}

}
