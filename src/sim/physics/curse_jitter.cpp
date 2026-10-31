#include "sim/physics/curse_jitter.hpp"

#include <algorithm>

namespace rr::sim {
namespace {

constexpr std::uint32_t kCountdownRange = 500;
constexpr std::uint32_t kSteerRange = 400;
constexpr std::uint32_t kThrottleRange = 200;
constexpr float kOffsetStep = 0.01F;
constexpr float kSteerBase = 2.0F;
constexpr float kThrottleBase = 1.0F;

float Offset(Random& random, std::uint32_t range, float base) {
    return (static_cast<float>(random.Below(range)) * kOffsetStep) - base;
}

}

void UpdateCurseJitter(CurseJitter& jitter, int dtMs, bool isCursed, Random& random) {
    if (dtMs <= jitter.countdownMs) {
        jitter.countdownMs -= dtMs;
        return;
    }
    if (isCursed) {
        jitter.countdownMs = static_cast<int>(random.Below(kCountdownRange));
        jitter.steerOffset = Offset(random, kSteerRange, kSteerBase);
        jitter.throttleOffset = Offset(random, kThrottleRange, kThrottleBase);
    }
}

DriveCommand Cursed(const DriveCommand& command, const CurseJitter& jitter) {
    DriveCommand cursed = command;
    cursed.steering = -command.steering;
    cursed.drive = std::clamp(command.drive + jitter.throttleOffset, -1.0F, 1.0F);
    return cursed;
}

}
