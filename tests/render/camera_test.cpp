#include "render/camera.hpp"

#include <doctest/doctest.h>

#include <numbers>

#include "core/quaternion.hpp"

#include <glm/vec4.hpp>

namespace rr::render {
namespace {

constexpr float kQuarterTurn = std::numbers::pi_v<float> / 2.0F;

glm::vec4 ToNdc(const glm::mat4& viewProjection, const glm::vec4& point) {
    const glm::vec4 clip = viewProjection * point;
    return clip / clip.w;
}

TEST_CASE("a point straight ahead projects to the center") {
    const Camera camera{.transform = {.position = {.z = 5.0F}}};

    const glm::vec4 ndc = ToNdc(ViewProjection(camera, 16.0F / 9.0F), {0.0F, 0.0F, -5.0F, 1.0F});

    CHECK(ndc.x == doctest::Approx(0.0F));
    CHECK(ndc.y == doctest::Approx(0.0F));
    CHECK(ndc.z > 0.0F);
    CHECK(ndc.z < 1.0F);
}

TEST_CASE("depth runs from zero at the near plane to one at the far plane") {
    const Camera camera{.nearPlane = 1.0F, .farPlane = 100.0F};
    const glm::mat4 viewProjection = ViewProjection(camera, 1.0F);

    CHECK(ToNdc(viewProjection, {0.0F, 0.0F, -1.0F, 1.0F}).z == doctest::Approx(0.0F));
    CHECK(ToNdc(viewProjection, {0.0F, 0.0F, -100.0F, 1.0F}).z == doctest::Approx(1.0F));
}

TEST_CASE("a turned camera sees along its new forward axis") {
    const Camera camera{.transform = {.rotation = FromAxisAngle({.y = 1.0F}, kQuarterTurn)}};

    const glm::vec4 ndc = ToNdc(ViewProjection(camera, 1.0F), {-10.0F, 0.0F, 0.0F, 1.0F});

    CHECK(ndc.x == doctest::Approx(0.0F));
    CHECK(ndc.z > 0.0F);
}

TEST_CASE("model matrices rotate before they translate") {
    const Transform transform{.position = {.x = 3.0F},
                              .rotation = FromAxisAngle({.z = 1.0F}, kQuarterTurn)};

    const glm::vec4 moved = ModelMatrix(transform) * glm::vec4(1.0F, 0.0F, 0.0F, 1.0F);

    CHECK(moved.x == doctest::Approx(3.0F));
    CHECK(moved.y == doctest::Approx(1.0F));
}

}
}
