#include "platform/frame_pacer.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

#include "platform/clock.hpp"

namespace rr::platform {

FramePacer::FramePacer(FrameTimer timer, PacingConfig config)
    : timer_(std::move(timer)), config_(config) {
    config_.fpsCap = std::clamp(config_.fpsCap, kMinFpsCap, kMaxFpsCap);
}

void FramePacer::WaitForNextFrame() {
    if (config_.mode != PresentMode::Capped) {
        return;
    }

    const Nanoseconds period = Nanoseconds{std::chrono::seconds{1}} / config_.fpsCap;
    const Nanoseconds now = Now();
    if (!lastSlot_ || now > *lastSlot_ + period) {
        lastSlot_ = now;
    }
    *lastSlot_ += period;
    timer_.SleepUntil(*lastSlot_);
}

}
