#include "app/menu/build_camera.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>

namespace rr::app {
namespace {

constexpr std::size_t kSlots = 8;
constexpr std::size_t kHomeSlot = 1;
constexpr std::size_t kHomePitch = 1;
constexpr float kFirstAngle = 1.57F;
constexpr float kTwoPi = 2.0F * std::numbers::pi_v<float>;
constexpr float kHalfPi = std::numbers::pi_v<float> / 2.0F;
constexpr float kSlotAngle = kTwoPi / static_cast<float>(kSlots);
constexpr Milliseconds kTurnTime{150};
constexpr Milliseconds kTiltTime{300};
constexpr float kNearRadius = 5.9F;
constexpr float kFarRadius = 8.5F;
constexpr std::array<Vector3, kBuildPitches> kNearEyes{{
    {.y = -14.0F, .z = 9.0F},
    {.y = -10.0F, .z = 14.0F},
    {.y = -0.3F, .z = 17.0F},
}};
constexpr std::array<Vector3, kBuildPitches> kFarEyes{{
    {.y = -18.0F, .z = 10.0F},
    {.y = -12.0F, .z = 17.0F},
    {.y = -0.3F, .z = 20.0F},
}};

float SlotAngle(std::size_t slot) {
    return std::fmod(kFirstAngle + (static_cast<float>(slot) * kSlotAngle), kTwoPi);
}

float Easing(Milliseconds left, Milliseconds total) {
    return std::sin(static_cast<float>(left.count()) / static_cast<float>(total.count()) * kHalfPi);
}

Milliseconds CountDown(Milliseconds left, Milliseconds elapsed) {
    return std::max(left - elapsed, Milliseconds{0});
}

Vector3 EyeAt(std::size_t pitch, float radius) {
    const float blend = std::clamp((radius - kNearRadius) / (kFarRadius - kNearRadius), 0.0F, 1.0F);
    return kNearEyes.at(pitch) + ((kFarEyes.at(pitch) - kNearEyes.at(pitch)) * blend);
}

}

bool BuildCamera::Turn(int step) {
    if (turnLeft_ > Milliseconds{0}) {
        return false;
    }
    turnFrom_ = SlotAngle(slot_);
    const auto count = static_cast<int>(kSlots);
    slot_ = static_cast<std::size_t>((((static_cast<int>(slot_) + step) % count) + count) % count);
    turnLeft_ = kTurnTime;
    return true;
}

bool BuildCamera::Tilt(int step) {
    const int pitch =
        std::clamp(static_cast<int>(pitch_) + step, 0, static_cast<int>(kBuildPitches) - 1);
    if (tiltLeft_ > Milliseconds{0} || std::cmp_equal(pitch, pitch_)) {
        return false;
    }
    tiltFrom_ = pitch_;
    pitch_ = static_cast<std::size_t>(pitch);
    tiltLeft_ = kTiltTime;
    return true;
}

bool BuildCamera::Reset() {
    if (tiltLeft_ > Milliseconds{0} ||
        !Turn(static_cast<int>(kHomeSlot + kSlots) - static_cast<int>(slot_))) {
        return false;
    }
    tiltFrom_ = pitch_;
    pitch_ = kHomePitch;
    tiltLeft_ = kTiltTime;
    return true;
}

void BuildCamera::Tick(Milliseconds elapsed) {
    turnLeft_ = CountDown(turnLeft_, elapsed);
    tiltLeft_ = CountDown(tiltLeft_, elapsed);
}

float BuildCamera::Angle() const {
    float target = SlotAngle(slot_);
    if (turnLeft_ == Milliseconds{0}) {
        return target;
    }
    if (target > turnFrom_ + kHalfPi) {
        target -= kTwoPi;
    } else if (target < turnFrom_ - kHalfPi) {
        target += kTwoPi;
    }
    return target + ((turnFrom_ - target) * Easing(turnLeft_, kTurnTime));
}

Vector3 BuildCamera::Eye(float radius) const {
    const Vector3 target = EyeAt(pitch_, radius);
    if (tiltLeft_ == Milliseconds{0}) {
        return target;
    }
    return target + ((EyeAt(tiltFrom_, radius) - target) * Easing(tiltLeft_, kTiltTime));
}

}
