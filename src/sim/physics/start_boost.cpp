#include "sim/physics/start_boost.hpp"

#include <algorithm>

namespace rr::sim {
namespace {

constexpr int kWindowMs = 100;
constexpr int kHoldMs = 2000;
constexpr int kStrongWindowMs = 60;

}

void StartBoost::HoldAtStart() {
    state_.isHeldAtStart = true;
}

std::optional<TurboLevel> StartBoost::Go() {
    state_.isHeldAtStart = false;
    if (state_.windowMs != 0) {
        return TakeWindow();
    }
    if (!state_.isThrottleHeld) {
        state_.windowMs = kWindowMs;
    }
    return std::nullopt;
}

std::optional<TurboLevel> StartBoost::Update(const RacerControls& controls, int dtMs) {
    state_.windowMs = std::max(state_.windowMs - dtMs, 0);
    state_.heldMs += dtMs;
    state_.isThrottleHeld = controls.isThrottleHeld;
    const int pedals = (controls.isThrottleHeld ? 1 : 0) - (controls.isBrakeHeld ? 1 : 0);
    if (pedals < 0) {
        state_.heldMs = 0;
        return std::nullopt;
    }
    if (pedals == 0) {
        return std::nullopt;
    }
    if (!state_.isHeldAtStart) {
        return state_.windowMs != 0 ? TakeWindow() : std::nullopt;
    }
    if (state_.heldMs > kHoldMs) {
        state_.windowMs = kWindowMs;
        state_.heldMs = 0;
    }
    return std::nullopt;
}

std::optional<TurboLevel> StartBoost::TakeWindow() {
    const TurboLevel level =
        state_.windowMs >= kStrongWindowMs ? TurboLevel::Strong : TurboLevel::Weak;
    state_.windowMs = 0;
    return level;
}

}
