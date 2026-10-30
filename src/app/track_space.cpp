#include "app/track_space.hpp"

#include <algorithm>

#include "core/quaternion.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace rr::app {
namespace {

constexpr float kMinLength = 1e-6F;
constexpr float kHalfRoot = 0.70710678F;
constexpr Quaternion kGameToEngine{.w = kHalfRoot, .x = -kHalfRoot, .y = 0.0F, .z = 0.0F};
constexpr Quaternion kEngineToGame{.w = kHalfRoot, .x = kHalfRoot, .y = 0.0F, .z = 0.0F};

glm::vec3 ToGlm(const Vector3& v) {
    return {v.x, v.y, v.z};
}

}

Vector3 ToEngine(const Vector3& game) {
    return {.x = game.x, .y = game.z, .z = -game.y};
}

Vector3 ToGame(const Vector3& engine) {
    return {.x = engine.x, .y = -engine.z, .z = engine.y};
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

Transform CarToEngine(const Transform& game) {
    const Vector3 right = Apply({.rotation = game.rotation}, {.y = -1.0F});
    const Vector3 up = Apply({.rotation = game.rotation}, {.z = 1.0F});
    return PlacementToEngine(game.position, right, up);
}

Transform TransformToEngine(const Transform& game) {
    return {.position = ToEngine(game.position),
            .rotation = kGameToEngine * game.rotation * kEngineToGame};
}

Transform TransformToGame(const Transform& engine) {
    return {.position = ToGame(engine.position),
            .rotation = kEngineToGame * engine.rotation * kGameToEngine};
}

Transform LookAt(const Vector3& eye, const Vector3& target) {
    const glm::vec3 direction = ToGlm(target) - ToGlm(eye);
    const glm::vec3 up{0.0F, 1.0F, 0.0F};
    if (glm::length(glm::cross(direction, up)) < kMinLength) {
        return {.position = eye};
    }
    const glm::quat rotation = glm::quatLookAt(glm::normalize(direction), up);
    return {.position = eye,
            .rotation = {.w = rotation.w, .x = rotation.x, .y = rotation.y, .z = rotation.z}};
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

}
