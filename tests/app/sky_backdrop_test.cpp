#include "app/sky_backdrop.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "assets/sky_file.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kCamera{.x = 5.0F, .y = 20.0F, .z = -3.0F};
constexpr render::MeshId kDome{7};
constexpr render::MeshId kPlanet{9};

assets::SkyFile Sky(float heightOffset = 0.0F) {
    return {.skies = {{.name = "daylight",
                       .keyframes = {{.horizon = {255, 255, 255},
                                      .middle = {0, 51, 102},
                                      .top = {255, 0, 0}}}},
                      {.name = "night", .keyframes = {{}}}},
            .heightOffset = heightOffset};
}

SkyBackdrop Backdrop() {
    return {kDome,
            {{.draws = {{.mesh = kPlanet, .indexCount = 6}}, .scroll = {.uPerSecond = 0.25F}}}};
}

std::vector<render::MeshDraw> Placed(const sim::RaceSky& sky, float shownMs = 0.0F) {
    std::vector<render::MeshDraw> draws{{.mesh = kPlanet}};
    Backdrop().Place(sky, kCamera, shownMs, draws);
    return draws;
}

bool IsOnHemisphere(const assets::GeometryVertex& vertex) {
    return std::abs(Length(vertex.position) - 100.0F) < 0.001F && vertex.position.y > -0.001F;
}

TEST_CASE("the sky dome is two bands of the original's 11-segment hemisphere") {
    const assets::GeometryAsset dome = SkyDomeGeometry();

    CHECK(dome.vertices.size() == 44);
    CHECK(dome.indices.size() == 132);
    REQUIRE(dome.ranges.size() == 2);
    CHECK(dome.ranges[1].firstIndex == 66);
    CHECK(std::ranges::all_of(dome.vertices, IsOnHemisphere));
    CHECK(dome.vertices[0].position.y == doctest::Approx(100.0F * std::cos(0.571198F)));
    CHECK(dome.vertices[0].rgba[0] == 255);
    CHECK(dome.vertices[11].rgba[0] == 0);
}

TEST_CASE("the dome sits below the camera by its height offset and blends each band") {
    const assets::SkyFile file = Sky(4.0F);
    const sim::RaceSky sky(file);

    const std::vector<render::MeshDraw> draws = Placed(sky);

    REQUIRE(draws.size() == 3);
    CHECK(draws[0].mesh == kDome);
    CHECK(draws[0].transform.position == Vector3{.x = 5.0F, .y = 14.0F, .z = -3.0F});
    CHECK(draws[0].firstIndex == 0);
    CHECK(draws[1].firstIndex == 66);
    const render::VertexColorTransform& top = draws[0].material.vertexColors;
    CHECK(top.offset.g == doctest::Approx(0.2F));
    CHECK(top.scale.r == doctest::Approx(1.0F));
    CHECK(top.scale.g == doctest::Approx(-0.2F));
    const render::VertexColorTransform& lower = draws[1].material.vertexColors;
    CHECK(lower.offset.b == doctest::Approx(1.0F));
    CHECK(lower.scale.b == doctest::Approx(-0.6F));
}

TEST_CASE("the sky world follows the camera and scrolls with the race time") {
    const assets::SkyFile file = Sky();
    const sim::RaceSky sky(file);

    const std::vector<render::MeshDraw> draws = Placed(sky, 1000.0F);

    REQUIRE(draws.size() == 3);
    CHECK(draws[2].mesh == kPlanet);
    CHECK(draws[2].transform.position == Vector3{.x = 5.0F, .y = 50.0F, .z = -3.0F});
    CHECK(draws[2].material.uvOffset.u == doctest::Approx(0.244140625F));
}

TEST_CASE("a hidden sky world leaves the dome, and a hidden dome leaves nothing") {
    const assets::SkyFile file = Sky();
    sim::RaceSky sky(file);

    sky.Apply({.hidesSkyWorld = true});
    CHECK(Placed(sky).size() == 2);
    sky.Apply({.hidesDome = true});
    CHECK(Placed(sky).empty());
}

TEST_CASE("a track without skies draws no backdrop") {
    const assets::SkyFile file;
    const sim::RaceSky sky(file);

    CHECK(Placed(sky).empty());
}

}
}
