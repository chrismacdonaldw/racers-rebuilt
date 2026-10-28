#include "app/track_space.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/quaternion.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace rr::app {
namespace {

constexpr float kMinLength = 1e-6F;
constexpr float kOverviewPitchRadians = std::numbers::pi_v<float> / 5.0F;
constexpr float kOverviewMargin = 1.3F;

glm::vec3 ToGlm(const Vector3& v) {
    return {v.x, v.y, v.z};
}

Vector3 FromGlm(const glm::vec3& v) {
    return {.x = v.x, .y = v.y, .z = v.z};
}

}

Vector3 ToEngine(const Vector3& game) {
    return {.x = game.x, .y = game.z, .z = -game.y};
}

Transform PlacementToEngine(const Vector3& position, const Vector3& forward, const Vector3& up) {
    const glm::vec3 front = ToGlm(ToEngine(forward));
    const glm::vec3 top = ToGlm(ToEngine(up));
    const Transform placed{.position = ToEngine(position)};
    if (glm::length(front) < kMinLength) {
        return placed;
    }
    const glm::vec3 x = glm::normalize(front);
    const glm::vec3 square = top - (x * glm::dot(top, x));
    if (glm::length(square) < kMinLength) {
        return placed;
    }
    const glm::vec3 y = glm::normalize(square);
    const glm::quat rotation = glm::quat_cast(glm::mat3(x, y, glm::cross(x, y)));
    return {.position = placed.position,
            .rotation = {.w = rotation.w, .x = rotation.x, .y = rotation.y, .z = rotation.z}};
}

Vector3 Apply(const Transform& transform, const Vector3& point) {
    const Quaternion& r = transform.rotation;
    return FromGlm(ToGlm(transform.position) + (glm::quat(r.w, r.x, r.y, r.z) * ToGlm(point)));
}

void Bounds::Add(const Vector3& point) {
    if (isEmpty) {
        min = point;
        max = point;
        isEmpty = false;
        return;
    }
    min = {.x = std::min(min.x, point.x),
           .y = std::min(min.y, point.y),
           .z = std::min(min.z, point.z)};
    max = {.x = std::max(max.x, point.x),
           .y = std::max(max.y, point.y),
           .z = std::max(max.z, point.z)};
}

Transform OverviewCamera(const Bounds& bounds, float verticalFovDegrees) {
    const glm::vec3 center = (ToGlm(bounds.min) + ToGlm(bounds.max)) / 2.0F;
    const glm::vec3 size = ToGlm(bounds.max) - ToGlm(bounds.min);
    const float footprintRadius = std::max(size.x, size.z) / 2.0F;
    const float distance =
        kOverviewMargin * footprintRadius / std::tan(glm::radians(verticalFovDegrees) / 2.0F);
    const glm::vec3 offset{0.0F, std::sin(kOverviewPitchRadians), std::cos(kOverviewPitchRadians)};
    return {.position = FromGlm(center + (offset * distance)),
            .rotation = FromAxisAngle({.x = 1.0F}, -kOverviewPitchRadians)};
}

}
