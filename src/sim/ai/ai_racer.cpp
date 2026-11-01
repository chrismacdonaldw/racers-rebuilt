#include "sim/ai/ai_racer.hpp"

#include <algorithm>
#include <cmath>
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
constexpr float kBoostSpeed = 1.7F;
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
constexpr float kSideReturnPerMs = 1.0F / 512.0F;
constexpr float kFullBump = 240.0F;
constexpr float kForwardBumpSwing = 0.05F;
constexpr float kBackwardBumpSwing = 0.25F;

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
    route_.Advance(elapsed, playbackSpeed_);
    if (effects_.isHovering || effects_.lift.height != 0.0F) {
        UpdateHoverLift(effects_.lift, dtMs);
    }
    const float sideReturn = kSideReturnPerMs * elapsedMs;
    sideOffset_ = std::clamp(0.0F, sideOffset_ - sideReturn, sideOffset_ + sideReturn);
    UpdateRouteHop(effects_, dtMs);
    Place(dtMs);
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

void AiRacer::Tilt(float ratePerMs, int durationMs) {
    StartRouteTilt(effects_.tilt, ratePerMs, durationMs);
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

void AiRacer::StartBoost() {
    if (effects_.isCurseSlowed || effects_.isGhost) {
        return;
    }
    effects_.isBoosting = true;
    playbackSpeed_ = kBoostSpeed;
}

void AiRacer::EndBoost() {
    if (!effects_.isBoosting || effects_.isGhost) {
        return;
    }
    effects_.isBoosting = false;
    ReturnToFullSpeed();
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

void AiRacer::HoldAt(float speed) {
    baseSpeed_ = speed;
    if (!effects_.isPushed) {
        targetSpeed_ = speed;
    }
}

void AiRacer::Release() {
    baseSpeed_ = 1.0F;
    if (!effects_.isPushed) {
        targetSpeed_ = 1.0F;
    }
}

void AiRacer::Shift(const Vector3& move) {
    route_.Seek(move);
    const float side = Dot(move, Rotate(pose_.rotation, {.y = 1.0F}));
    sideOffset_ = std::clamp(side, -route_.WidthLeft(), route_.WidthRight());
    Place(0);
}

void AiRacer::Bump(const Vector3& direction, float impulse) {
    float along = Dot(Facing(), direction);
    if (impulse < 0.0F) {
        along = -along;
    }
    const float share = std::min(std::abs(impulse), kFullBump) / kFullBump;
    if (along >= 0.0F) {
        const float speed =
            effects_.isSpinning ? kSpinSpeed + kSpinPushSwing : playbackSpeed_ + share;
        playbackSpeed_ = std::min(speed + ((1.0F - along) * kForwardBumpSwing), kMaxSpeed);
    } else {
        const float speed =
            effects_.isSpinning ? kSpinSpeed - kSpinPushSwing : playbackSpeed_ - share;
        playbackSpeed_ = std::max(speed - ((along + 1.0F) * kBackwardBumpSwing), kMinSpeed);
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

void AiRacer::Place(int dtMs) {
    pose_ = PlaceOnRoute(effects_, route_.Pose(), wheelAnchor_, dtMs);
    pose_.position += Rotate(pose_.rotation, {.y = 1.0F}) * sideOffset_;
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
