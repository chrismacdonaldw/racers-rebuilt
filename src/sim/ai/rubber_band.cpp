#include "sim/ai/rubber_band.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>

#include "core/assert.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kDelay{15000};
constexpr float kPull = 0.05F;
constexpr float kMirroredBonus = 0.05F;

std::optional<float> BestPlayerProgress(std::span<const PacedRacer> racers) {
    std::optional<float> best;
    for (const PacedRacer& racer : racers) {
        if (!racer.isAiControlled) {
            best = std::max(best.value_or(racer.lapProgress), racer.lapProgress);
        }
    }
    return best;
}

}

RubberBand::RubberBand(bool isMirrored)
    : delay_(kDelay), bonus_(isMirrored ? kMirroredBonus : 0.0F) {}

void RubberBand::Update(Milliseconds elapsed, std::span<const PacedRacer> racers,
                        std::span<float> targetSpeeds) {
    RR_ASSERT(racers.size() == targetSpeeds.size());
    if (elapsed <= delay_) {
        delay_ -= elapsed;
        return;
    }
    delay_ = Milliseconds{0};
    Pace(racers, targetSpeeds);
}

void RubberBand::Pace(std::span<const PacedRacer> racers, std::span<float> targetSpeeds) const {
    const std::optional<float> best = BestPlayerProgress(racers);
    if (!best) {
        std::ranges::fill(targetSpeeds, 1.0F);
        return;
    }
    for (std::size_t i = 1; i < racers.size(); ++i) {
        if (racers[i].lapProgress > *best) {
            targetSpeeds[i] = 1.0F - kPull + bonus_;
        } else if (racers[i].lapProgress < *best) {
            targetSpeeds[i] = 1.0F + kPull + bonus_;
        }
    }
}

}
