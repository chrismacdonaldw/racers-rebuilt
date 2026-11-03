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

constexpr auto kTopPitch = static_cast<float>(kBuildPitches - 1);

float SlotAngle(float slot) {
    return std::fmod(kFirstAngle + (slot * kSlotAngle), kTwoPi);
}

float SlotAngle(std::size_t slot) {
    return SlotAngle(static_cast<float>(slot));
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

Vector3 EyeAt(float pitch, float radius) {
    const auto low = std::min(static_cast<std::size_t>(pitch), kBuildPitches - 2);
    const Vector3 from = EyeAt(low, radius);
    return from + ((EyeAt(low + 1, radius) - from) * (pitch - static_cast<float>(low)));
}

float DistanceSquared(const Vector3& a, const Vector3& b) {
    const Vector3 apart = a - b;
    return (apart.x * apart.x) + (apart.y * apart.y) + (apart.z * apart.z);
}

}

bool BuildCamera::Turn(int step) {
    if (turnLeft_ > Milliseconds{0}) {
        return false;
    }
    turnFrom_ = SlotAngle(slot_);
    const auto count = static_cast<int>(kSlots);
    slot_ = static_cast<std::size_t>((((static_cast<int>(slot_) + step) % count) + count) % count);
    place_ = static_cast<float>(slot_);
    turnLeft_ = kTurnTime;
    return true;
}

bool BuildCamera::Swing(float slots) {
    if (turnLeft_ > Milliseconds{0} || slots == 0.0F) {
        return false;
    }
    constexpr auto kCount = static_cast<float>(kSlots);
    place_ = std::fmod(std::fmod(place_ + slots, kCount) + kCount, kCount);
    return true;
}

bool BuildCamera::Lift(float tilts) {
    if (tiltLeft_ > Milliseconds{0} || tilts == 0.0F || (tilts > 0.0F && tilt_ >= kTopPitch) ||
        (tilts < 0.0F && tilt_ <= 0.0F)) {
        return false;
    }
    tilt_ = std::clamp(tilt_ + tilts, 0.0F, kTopPitch);
    return true;
}

void BuildCamera::Settle(float radius) {
    if (turnLeft_ == Milliseconds{0}) {
        const std::size_t nearest = static_cast<std::size_t>(std::lround(place_)) % kSlots;
        if (nearest != slot_ || place_ != static_cast<float>(nearest)) {
            turnFrom_ = SlotAngle(place_);
            slot_ = nearest;
            place_ = static_cast<float>(nearest);
            turnLeft_ = kTurnTime;
        }
    }
    if (tiltLeft_ > Milliseconds{0}) {
        return;
    }
    const Vector3 eye = EyeAt(tilt_, radius);
    std::size_t nearest = 0;
    for (std::size_t pitch = 1; pitch < kBuildPitches; ++pitch) {
        if (DistanceSquared(eye, EyeAt(pitch, radius)) <
            DistanceSquared(eye, EyeAt(nearest, radius))) {
            nearest = pitch;
        }
    }
    if (tilt_ != static_cast<float>(nearest)) {
        tiltFrom_ = tilt_;
        tiltLeft_ = kTiltTime;
    }
    pitch_ = nearest;
    tilt_ = static_cast<float>(nearest);
}

bool BuildCamera::Tilt(int step) {
    const int pitch =
        std::clamp(static_cast<int>(pitch_) + step, 0, static_cast<int>(kBuildPitches) - 1);
    if (tiltLeft_ > Milliseconds{0} || std::cmp_equal(pitch, pitch_)) {
        return false;
    }
    tiltFrom_ = static_cast<float>(pitch_);
    pitch_ = static_cast<std::size_t>(pitch);
    tilt_ = static_cast<float>(pitch_);
    tiltLeft_ = kTiltTime;
    return true;
}

bool BuildCamera::Reset() {
    if (tiltLeft_ > Milliseconds{0} ||
        !Turn(static_cast<int>(kHomeSlot + kSlots) - static_cast<int>(slot_))) {
        return false;
    }
    tiltFrom_ = static_cast<float>(pitch_);
    pitch_ = kHomePitch;
    tilt_ = static_cast<float>(pitch_);
    tiltLeft_ = kTiltTime;
    return true;
}

void BuildCamera::Tick(Milliseconds elapsed) {
    turnLeft_ = CountDown(turnLeft_, elapsed);
    tiltLeft_ = CountDown(tiltLeft_, elapsed);
}

float BuildCamera::Angle() const {
    if (turnLeft_ == Milliseconds{0}) {
        return SlotAngle(place_);
    }
    float target = SlotAngle(slot_);
    if (target > turnFrom_ + kHalfPi) {
        target -= kTwoPi;
    } else if (target < turnFrom_ - kHalfPi) {
        target += kTwoPi;
    }
    return target + ((turnFrom_ - target) * Easing(turnLeft_, kTurnTime));
}

Vector3 BuildCamera::Eye(float radius) const {
    const Vector3 target = EyeAt(tilt_, radius);
    if (tiltLeft_ == Milliseconds{0}) {
        return target;
    }
    return target + ((EyeAt(tiltFrom_, radius) - target) * Easing(tiltLeft_, kTiltTime));
}

}
