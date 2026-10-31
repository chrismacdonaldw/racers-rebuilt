#include "sim/ai/ai_racer.hpp"

#include <algorithm>
#include <utility>

#include "core/quaternion.hpp"
#include "sim/physics/hover_contact.hpp"

namespace rr::sim {
namespace {

constexpr float kSpeedUpPerMs = 1.0F / 2048.0F;
constexpr float kSlowDownPerMs = 0.002F;
constexpr float kBoostSpeedUpPerMs = 0.01F;
constexpr float kPushedSpeedUpPerMs = 0.002F;
constexpr float kPushedSlowDownPerMs = 0.000732421875F;
constexpr float kTwoPi = 6.2831855F;
constexpr float kSpinSpeed = 0.3F;
constexpr float kSpinPushSwing = 0.1F;
constexpr float kSpinOutSpeed = 0.1F;
constexpr float kSpinOutHopSpeed = 50.0F;
constexpr float kCurseSpeed = 0.5F;
constexpr float kGhostSpeed = 4.5F;
constexpr float kMaxSpeed = 2.75F;
constexpr float kMinSpeed = -0.5F;
constexpr float kPushPerSpeed = 300.0F;
constexpr float kHoverHeight = 6.0F;
constexpr float kHoverRisePerSecond = 3.0F;
constexpr float kHoverDropPerSecond = 15.0F;
constexpr float kHoverTiltRate = -0.003F;
constexpr int kHoverTiltMs = 150;
constexpr float kBankLookAheadMs = 500.0F;
constexpr float kBankPerTurn = 8.0F;
constexpr float kMaxBank = 0.7853982F;

}

AiRacer::AiRacer(assets::RaceRoute route, const Vector3& wheelAnchor)
    : route_(std::move(route)), wheelAnchor_(wheelAnchor), pose_(route_.Pose()) {}

void AiRacer::Tick(Milliseconds elapsed) {
    if (elapsed <= Milliseconds{0}) {
        return;
    }
    const auto dtMs = static_cast<int>(elapsed.count());
    const auto elapsedMs = static_cast<float>(dtMs);
    if (effects_.isHovering) {
        AimBank();
    }
    if (!HoldsSpeed()) {
        EaseTowardTarget(elapsedMs);
    }
    const Vector3 from = pose_.position;
    route_.Advance(elapsed, std::max(playbackSpeed_, 0.0F));
    if (effects_.isHovering || effects_.lift.height != 0.0F) {
        UpdateHoverLift(effects_.lift, dtMs);
    }
    UpdateRouteHop(effects_, dtMs);
    pose_ = PlaceOnRoute(effects_, route_.Pose(), wheelAnchor_, dtMs);
    velocity_ = (pose_.position - from) * (1.0F / elapsedMs);
}

void AiRacer::SetBaseSpeed(float speed) {
    baseSpeed_ = speed;
    if (!effects_.isPushed) {
        targetSpeed_ = speed;
    }
}

void AiRacer::StartSpin(float turns, float ratePerMs) {
    if (effects_.isSpinning) {
        return;
    }
    effects_.spin = {.remainingMs = static_cast<int>(turns * kTwoPi / ratePerMs),
                     .ratePerMs = ratePerMs,
                     .angle = 0.0F};
    effects_.isSpinning = true;
    if (ContactCount() != 0) {
        playbackSpeed_ = kSpinSpeed;
    }
}

void AiRacer::EndSpin() {
    effects_.isSpinning = false;
}

void AiRacer::StartSpinOut() {
    effects_.isSpunOut = true;
    effects_.hop.velocity = kSpinOutHopSpeed;
    playbackSpeed_ = kSpinOutSpeed;
}

void AiRacer::StartHover() {
    if (effects_.isHovering) {
        return;
    }
    StartRouteTilt(effects_.tilt, kHoverTiltRate, kHoverTiltMs);
    effects_.isHovering = true;
    effects_.lift = {.height = 0.0F,
                     .targetHeight = kHoverHeight,
                     .ratePerSecond = kHoverRisePerSecond,
                     .bank = 0.0F,
                     .targetBank = 0.0F};
}

void AiRacer::StopHover() {
    effects_.isHovering = false;
    effects_.lift.targetHeight = 0.0F;
    effects_.lift.targetBank = 0.0F;
    effects_.lift.ratePerSecond = kHoverDropPerSecond;
}

void AiRacer::StartPush(const Vector3& force) {
    effects_.isPushed = true;
    const float along = Dot(Facing(), force);
    if (along >= 0.0F) {
        const float target = effects_.isSpinning ? kSpinSpeed + kSpinPushSwing
                                                 : (along / kPushPerSpeed) + playbackSpeed_;
        targetSpeed_ = std::min(target, kMaxSpeed);
    } else {
        const float target = effects_.isSpinning ? kSpinSpeed - kSpinPushSwing
                                                 : (along / kPushPerSpeed) + playbackSpeed_;
        targetSpeed_ = std::max(target, kMinSpeed);
    }
}

void AiRacer::EndPush() {
    effects_.isPushed = false;
    if (effects_.isSpinning) {
        targetSpeed_ = kSpinSpeed;
        return;
    }
    if (baseSpeed_ < 0.0F) {
        baseSpeed_ = 1.0F;
    }
    targetSpeed_ = baseSpeed_;
}

void AiRacer::StartCurseSlow() {
    if (effects_.isBoosting) {
        effects_.isBoosting = false;
        ReturnToFullSpeed();
    }
    effects_.isCurseSlowed = true;
    playbackSpeed_ = kCurseSpeed;
}

void AiRacer::EndCurseSlow() {
    effects_.isCurseSlowed = false;
    ReturnToFullSpeed();
}

void AiRacer::StartGhost() {
    effects_.isHovering = true;
    effects_.isBoosting = true;
    effects_.isGhost = true;
    playbackSpeed_ = kGhostSpeed;
    effects_.lift.targetHeight = kHoverHeight;
    effects_.lift.targetBank = 0.0F;
    effects_.lift.ratePerSecond = kHoverRisePerSecond;
}

void AiRacer::EndGhost() {
    playbackSpeed_ = baseSpeed_;
    effects_.lift.targetHeight = 0.0F;
    effects_.lift.targetBank = 0.0F;
    effects_.lift.ratePerSecond = kHoverDropPerSecond;
    effects_.isHovering = false;
    effects_.isBoosting = false;
    effects_.isGhost = false;
}

void AiRacer::Hold() {
    baseSpeed_ = 0.0F;
    if (!effects_.isPushed) {
        targetSpeed_ = 0.0F;
    }
}

void AiRacer::Release() {
    baseSpeed_ = 1.0F;
    if (!effects_.isPushed) {
        targetSpeed_ = 1.0F;
    }
}

Vector3 AiRacer::Facing() const {
    return Rotate(pose_.rotation, {.x = 1.0F});
}

void AiRacer::EaseTowardTarget(float elapsedMs) {
    const bool isPushed = effects_.isPushed;
    if (playbackSpeed_ < targetSpeed_) {
        float rate = kSpeedUpPerMs;
        if (isPushed) {
            rate = kPushedSpeedUpPerMs;
        } else if (effects_.isBoosting) {
            rate = kBoostSpeedUpPerMs;
        }
        playbackSpeed_ = std::min(playbackSpeed_ + (rate * elapsedMs), targetSpeed_);
    } else {
        const float rate = isPushed ? kPushedSlowDownPerMs : kSlowDownPerMs;
        playbackSpeed_ = std::max(playbackSpeed_ - (rate * elapsedMs), targetSpeed_);
    }
}

void AiRacer::AimBank() {
    const Quaternion& now = route_.Pose().rotation;
    const Quaternion ahead =
        route_.PoseAhead(kBankLookAheadMs * std::max(playbackSpeed_, 0.0F)).rotation;
    const float turn = Dot(Rotate(ahead, {.x = 1.0F}), Rotate(now, {.y = 1.0F}));
    effects_.lift.targetBank = -kMaxBank * std::clamp(turn * kBankPerTurn, -1.0F, 1.0F);
}

bool AiRacer::HoldsSpeed() const {
    const bool isOverridden = effects_.isSpinning || effects_.isBoosting || effects_.isCurseSlowed;
    return !effects_.isPushed && isOverridden && ContactCount() != 0;
}

void AiRacer::ReturnToFullSpeed() {
    if (!effects_.isSpinning && !effects_.isBoosting && !effects_.isPushed) {
        targetSpeed_ = 1.0F;
    }
}

}
