#include "app/free_camera.hpp"

#include <SDL3/SDL_keyboard.h>

#include <algorithm>
#include <cmath>

namespace rr::app {
namespace {

constexpr float kMoveUnitsPerSecond = 5.0F;
constexpr float kFastMultiplier = 4.0F;
constexpr float kLookRadiansPerSecond = 1.5F;
constexpr float kMaxPitchRadians = 1.5F;

float Axis(bool positive, bool negative) {
    return (positive ? 1.0F : 0.0F) - (negative ? 1.0F : 0.0F);
}

Vector3 Forward(const Quaternion& q) {
    return {.x = -2.0F * ((q.x * q.z) + (q.w * q.y)),
            .y = -2.0F * ((q.y * q.z) - (q.w * q.x)),
            .z = -(1.0F - (2.0F * ((q.x * q.x) + (q.y * q.y))))};
}

Quaternion FromYawPitch(float yaw, float pitch) {
    const float cy = std::cos(yaw / 2.0F);
    const float sy = std::sin(yaw / 2.0F);
    const float cp = std::cos(pitch / 2.0F);
    const float sp = std::sin(pitch / 2.0F);
    return {.w = cy * cp, .x = cy * sp, .y = sy * cp, .z = -sy * sp};
}

}

FlyKeys ReadFlyKeys() {
    const bool* held = SDL_GetKeyboardState(nullptr);
    return {.forward = held[SDL_SCANCODE_W],
            .back = held[SDL_SCANCODE_S],
            .left = held[SDL_SCANCODE_A],
            .right = held[SDL_SCANCODE_D],
            .down = held[SDL_SCANCODE_Q],
            .up = held[SDL_SCANCODE_E],
            .lookLeft = held[SDL_SCANCODE_LEFT],
            .lookRight = held[SDL_SCANCODE_RIGHT],
            .lookUp = held[SDL_SCANCODE_UP],
            .lookDown = held[SDL_SCANCODE_DOWN],
            .fast = held[SDL_SCANCODE_LSHIFT] || held[SDL_SCANCODE_RSHIFT]};
}

void FreeCamera::DeclareSettings(settings::Registry& registry) const {
    registry.Declare(kFreeCamera);
}

void FreeCamera::Attach(mods::HookRegistry& hooks, const settings::Registry& settings) {
    isEnabled_ = settings.Get(kFreeCamera);
    hooks.Subscribe<mods::SettingChanged>([this](const mods::SettingChanged& changed) {
        if (changed.key == kFreeCamera.key) {
            isEnabled_ = changed.settings.Get(kFreeCamera);
            hasPose_ = false;
        }
    });
    hooks.Subscribe<mods::SceneEntered>([this](const mods::SceneEntered&) { hasPose_ = false; });
    hooks.Subscribe<mods::FrameRendering>([this](mods::FrameRendering& frame) { Fly(frame); });
}

void FreeCamera::Fly(mods::FrameRendering& frame) {
    if (!isEnabled_) {
        return;
    }
    if (!hasPose_) {
        const Vector3 forward = Forward(frame.camera.rotation);
        position_ = frame.camera.position;
        yawRadians_ = std::atan2(-forward.x, -forward.z);
        pitchRadians_ = std::asin(std::clamp(forward.y, -1.0F, 1.0F));
        hasPose_ = true;
    }
    const FlyKeys keys = readKeys_();
    const auto seconds = static_cast<float>(frame.elapsed.count());
    const float look = kLookRadiansPerSecond * seconds;
    yawRadians_ += Axis(keys.lookLeft, keys.lookRight) * look;
    pitchRadians_ = std::clamp(pitchRadians_ + (Axis(keys.lookUp, keys.lookDown) * look),
                               -kMaxPitchRadians, kMaxPitchRadians);

    const Quaternion rotation = FromYawPitch(yawRadians_, pitchRadians_);
    const Vector3 forward = Forward(rotation);
    const Vector3 right{.x = std::cos(yawRadians_), .z = -std::sin(yawRadians_)};
    const float step = kMoveUnitsPerSecond * (keys.fast ? kFastMultiplier : 1.0F) * seconds;
    const float ahead = Axis(keys.forward, keys.back) * step;
    const float aside = Axis(keys.right, keys.left) * step;
    position_ = {.x = position_.x + (forward.x * ahead) + (right.x * aside),
                 .y = position_.y + (forward.y * ahead) + (Axis(keys.up, keys.down) * step),
                 .z = position_.z + (forward.z * ahead) + (right.z * aside)};
    frame.camera = {.position = position_, .rotation = rotation};
}

}
