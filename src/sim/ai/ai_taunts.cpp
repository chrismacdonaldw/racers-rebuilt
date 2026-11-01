#include "sim/ai/ai_taunts.hpp"

#include <algorithm>
#include <cstdint>

namespace rr::sim {
namespace {

constexpr int kMsPerRandomStep = 8;

}

bool AiTaunter::Tick(Milliseconds elapsed, std::span<const Racer> racers, std::size_t racer,
                     Random& random) {
    wait_ = std::max(wait_ - elapsed, Milliseconds{0});
    if (wait_ > Milliseconds{0}) {
        return false;
    }
    const BodyMotion& motion = racers[racer].Body().motion;
    if (!FindNearestRacerInCone(racers, motion.position, motion.basis.forward, kTauntCone)) {
        wait_ = kTauntRetry;
        return false;
    }
    wait_ =
        kTauntRetry + Milliseconds{kMsPerRandomStep * static_cast<int>(random.Below(kRollRange))};
    return true;
}

}
