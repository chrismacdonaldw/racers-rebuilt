#pragma once

#include <span>
#include <string_view>

#include "core/duration.hpp"
#include "sim/physics/version_rules.hpp"

namespace rr::sim {

struct PacedRacer {
    float lapProgress = 0.0F;
    bool isAiControlled = false;
};

/// After 15 s, racers but the first play 5% slower ahead of the best player, 5% faster behind.
class RubberBand {
public:
    explicit RubberBand(float bonus);

    /// Rewrites `targetSpeeds`, one per racer, where pacing changes them.
    void Update(Milliseconds elapsed, std::span<const PacedRacer> racers,
                std::span<float> targetSpeeds);

private:
    void Pace(std::span<const PacedRacer> racers, std::span<float> targetSpeeds) const;

    Milliseconds delay_;
    float bonus_ = 0.0F;
};

[[nodiscard]] float PaceBonus(bool isMirrored, std::string_view circuit, const VersionRules& rules);

}
