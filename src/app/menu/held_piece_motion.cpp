#include "app/menu/held_piece_motion.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace rr::app {
namespace {

constexpr float kFloat = 1.2F;
constexpr float kTrailTop = kHoverHeight - (2.0F * kFloat);
constexpr float kRisePerMs = 0.01F;
constexpr float kLiftPerMs = 0.006F;
constexpr float kSlowestFall = 0.3F;
constexpr float kFallDistance = 3.0F;
constexpr Milliseconds kTrailPhase{2500};
constexpr Milliseconds kDropTime{2500};
constexpr Milliseconds kLiftTime{2000};
constexpr Milliseconds kBobPeriod{1000};
constexpr float kTrailAlpha = 64.0F / 255.0F;
constexpr float kGhostAlpha = 150.0F / 255.0F;
constexpr float kBobAlpha = 100.0F / 255.0F;
constexpr float kBobSwing = 50.0F / 255.0F;

float Ms(Milliseconds time) {
    return static_cast<float>(time.count());
}

}

PieceMotionEnd HeldPieceMotion::Tick(Milliseconds elapsed, bool fits, float rest) {
    if (!IsBusy()) {
        MoveTrail(elapsed, fits, rest);
    }
    bob_ = elapsed >= bob_ ? bob_ - elapsed + kBobPeriod : bob_ - elapsed;
    return IsBusy() ? MoveBusy(elapsed) : PieceMotionEnd::None;
}

void HeldPieceMotion::Drop() {
    busy_ = Busy::Dropping;
    busyLeft_ = kDropTime;
    busyHeight_ = kHoverHeight;
}

void HeldPieceMotion::Lift(float rest) {
    busy_ = Busy::Lifting;
    busyLeft_ = kLiftTime;
    rest_ = rest;
    busyHeight_ = rest;
}

void HeldPieceMotion::MoveTrail(Milliseconds elapsed, bool fits, float rest) {
    if (!fits) {
        trail_ = Trail::Off;
    } else if (trail_ == Trail::Off) {
        trail_ = Trail::Lowering;
        trailLeft_ = kTrailPhase;
    }
    StepTrail(elapsed);
    rest_ = rest;
    if (trail_ == Trail::Lowering) {
        const float fall =
            std::clamp(std::trunc(trailHeight_ - rest_) / kFallDistance, kSlowestFall, 1.0F);
        trailHeight_ -= fall * kRisePerMs * Ms(elapsed);
        if (trailHeight_ <= rest_) {
            trailLeft_ = Milliseconds{0};
            trailHeight_ = rest_;
        }
    } else if (trail_ == Trail::Raising) {
        trailHeight_ += kRisePerMs * Ms(elapsed);
        if (trailHeight_ >= kTrailTop) {
            trailLeft_ = Milliseconds{0};
            trailHeight_ = kTrailTop;
        }
    } else if (trail_ == Trail::Off) {
        trailLeft_ = kTrailPhase;
        trailHeight_ = rest_;
    }
}

void HeldPieceMotion::StepTrail(Milliseconds elapsed) {
    if (trail_ == Trail::Off) {
        return;
    }
    if (elapsed < trailLeft_) {
        trailLeft_ -= elapsed;
        return;
    }
    const Milliseconds over = elapsed - trailLeft_;
    switch (trail_) {
        case Trail::Lowering:
            trail_ = Trail::Holding;
            trailLeft_ = Milliseconds{0};
            break;
        case Trail::Holding:
            trail_ = Trail::Raising;
            trailLeft_ = kTrailPhase - over;
            break;
        case Trail::Raising:
            trail_ = Trail::Lowering;
            trailLeft_ = kTrailPhase - over;
            break;
        case Trail::Off:
            break;
    }
}

PieceMotionEnd HeldPieceMotion::MoveBusy(Milliseconds elapsed) {
    const bool isDropping = busy_ == Busy::Dropping;
    if (elapsed >= busyLeft_) {
        busy_ = Busy::None;
        busyLeft_ = Milliseconds{0};
        return isDropping ? PieceMotionEnd::Dropped : PieceMotionEnd::Lifted;
    }
    busyLeft_ -= elapsed;
    if (isDropping) {
        busyHeight_ -= kRisePerMs * Ms(elapsed);
        if (busyHeight_ < rest_) {
            busyLeft_ = Milliseconds{0};
            busyHeight_ = rest_;
        }
    } else {
        busyHeight_ += kLiftPerMs * Ms(elapsed);
        if (busyHeight_ > kHoverHeight) {
            busyLeft_ = Milliseconds{0};
            busyHeight_ = kHoverHeight;
        }
    }
    return PieceMotionEnd::None;
}

float HeldPieceMotion::GhostAlpha(bool isOverhead) const {
    if (trail_ == Trail::Off) {
        const float turn = Ms(bob_) / Ms(kBobPeriod) * 2.0F * std::numbers::pi_v<float>;
        return kBobAlpha + (kBobSwing * std::sin(turn));
    }
    const float above = trailHeight_ - rest_ - kFloat;
    if (above >= kFloat || isOverhead) {
        return kGhostAlpha;
    }
    return std::max(above, 0.0F) / kFloat * kGhostAlpha;
}

HeldPieceLayers HeldPieceMotion::Layers(bool isOverhead) const {
    if (IsBusy()) {
        return {.solid = busyHeight_, .trail = std::nullopt, .ghost = std::nullopt};
    }
    HeldPieceLayers layers;
    if (!isOverhead) {
        layers.solid = kHoverHeight;
        if (trail_ != Trail::Off) {
            layers.trail = PieceLayer{.height = trailHeight_, .alpha = kTrailAlpha};
        }
    }
    if (const float alpha = GhostAlpha(isOverhead); alpha > 0.0F) {
        layers.ghost = PieceLayer{.height = rest_, .alpha = alpha};
    }
    return layers;
}

}
