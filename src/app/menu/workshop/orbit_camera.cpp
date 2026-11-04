#include "app/menu/workshop/orbit_camera.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace rr::app {
namespace {

constexpr float kLowestPitch = 0.05F;
constexpr float kHighestPitch = (std::numbers::pi_v<float> / 2.0F) - 0.01F;
constexpr float kNearest = 3.0F;
constexpr float kFarthest = 90.0F;
constexpr float kFocusReach = 24.0F;
constexpr float kFocusTop = 12.0F;
constexpr float kZoomPerStep = 0.85F;
constexpr float kEaseMs = 60.0F;
constexpr float kFrameMargin = 1.15F;
constexpr float kDegrees = 180.0F;

Vector3 Along(float yaw, float pitch) {
    return {.x = std::cos(pitch) * std::cos(yaw),
            .y = std::cos(pitch) * std::sin(yaw),
            .z = std::sin(pitch)};
}

float Ease(float from, float to, float share) {
    return from + ((to - from) * share);
}

}

OrbitCamera::OrbitCamera(OrbitPose home) : home_(home), goal_(home) {
    Clamp();
    home_ = goal_;
    shown_ = goal_;
}

void OrbitCamera::Turn(float yaw, float pitch) {
    goal_.yaw += yaw;
    goal_.pitch += pitch;
    Clamp();
}

void OrbitCamera::Pan(float right, float up) {
    const Vector3 back = Along(goal_.yaw, goal_.pitch);
    const Vector3 across{.x = -std::sin(goal_.yaw), .y = std::cos(goal_.yaw)};
    const Vector3 raised = Cross(back, across);
    goal_.focus =
        goal_.focus - (across * (right * goal_.distance)) - (raised * (up * goal_.distance));
    Clamp();
}

void OrbitCamera::Zoom(float steps) {
    goal_.distance *= std::pow(kZoomPerStep, steps);
    Clamp();
}

void OrbitCamera::Frame(const BoundingSphere& sphere, float verticalFovDegrees) {
    const float halfFov = verticalFovDegrees * std::numbers::pi_v<float> / (2.0F * kDegrees);
    goal_.focus = sphere.centre;
    goal_.distance = kFrameMargin * sphere.radius / std::sin(halfFov);
    Clamp();
}

void OrbitCamera::Tick(Milliseconds elapsed) {
    const float share = 1.0F - std::exp(-static_cast<float>(elapsed.count()) / kEaseMs);
    shown_ = {.focus = {.x = Ease(shown_.focus.x, goal_.focus.x, share),
                        .y = Ease(shown_.focus.y, goal_.focus.y, share),
                        .z = Ease(shown_.focus.z, goal_.focus.z, share)},
              .yaw = Ease(shown_.yaw, goal_.yaw, share),
              .pitch = Ease(shown_.pitch, goal_.pitch, share),
              .distance = Ease(shown_.distance, goal_.distance, share)};
}

Vector3 OrbitCamera::Eye() const {
    return shown_.focus + (Along(shown_.yaw, shown_.pitch) * shown_.distance);
}

void OrbitCamera::Clamp() {
    goal_.pitch = std::clamp(goal_.pitch, kLowestPitch, kHighestPitch);
    goal_.distance = std::clamp(goal_.distance, kNearest, kFarthest);
    goal_.focus = {.x = std::clamp(goal_.focus.x, -kFocusReach, kFocusReach),
                   .y = std::clamp(goal_.focus.y, -kFocusReach, kFocusReach),
                   .z = std::clamp(goal_.focus.z, 0.0F, kFocusTop)};
}

}
