#include "render/light_uniforms.hpp"

#include <doctest/doctest.h>

#include <array>

#include "render/render_view.hpp"

#include <glm/vec4.hpp>

namespace rr::render {
namespace {

TEST_CASE("a default view keeps the one white light it always had") {
    const LightUniforms uniforms = ToLightUniforms({});

    REQUIRE(uniforms.count == 1);
    const glm::vec4 direction = uniforms.directions[0];
    CHECK(direction.x == doctest::Approx(-0.3602F).epsilon(0.001));
    CHECK(direction.y == doctest::Approx(-0.8605F).epsilon(0.001));
    CHECK(direction.z == doctest::Approx(-0.3602F).epsilon(0.001));
    CHECK(direction.w == 0.0F);
    CHECK(uniforms.colors[0] == glm::vec4(1.0F));
}

TEST_CASE("coloured lights keep their order and colour, with unit directions") {
    const std::array lights{
        DirectionalLight{.direction = {.z = -2.0F}, .color = {.r = 1.0F, .g = 0.0F, .b = 0.0F}},
        DirectionalLight{.direction = {.x = 3.0F}, .color = {.r = 0.0F, .g = 0.5F, .b = 0.25F}},
    };

    const LightUniforms uniforms = ToLightUniforms({.lights = lights});

    REQUIRE(uniforms.count == 2);
    CHECK(uniforms.directions[0] == glm::vec4(0.0F, 0.0F, -1.0F, 0.0F));
    CHECK(uniforms.directions[1] == glm::vec4(1.0F, 0.0F, 0.0F, 0.0F));
    CHECK(uniforms.colors[0] == glm::vec4(1.0F, 0.0F, 0.0F, 1.0F));
    CHECK(uniforms.colors[1] == glm::vec4(0.0F, 0.5F, 0.25F, 1.0F));
}

TEST_CASE("a view can have no lights, or the most the shader takes") {
    CHECK(ToLightUniforms({.lights = {}}).count == 0);

    std::array<DirectionalLight, kMaxDirectionalLights> lights{};
    lights.fill({.direction = {.y = 1.0F}});
    const LightUniforms uniforms = ToLightUniforms({.lights = lights});
    CHECK(uniforms.count == kMaxDirectionalLights);
    CHECK(uniforms.directions.back() == glm::vec4(0.0F, 1.0F, 0.0F, 0.0F));
}

TEST_CASE("a light with no direction adds nothing") {
    const std::array lights{DirectionalLight{}};

    const LightUniforms uniforms = ToLightUniforms({.lights = lights});

    CHECK(uniforms.count == 1);
    CHECK(uniforms.colors[0] == glm::vec4(0.0F));
}

}
}
