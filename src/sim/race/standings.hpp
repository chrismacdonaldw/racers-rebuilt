#pragma once

#include <cstddef>
#include <optional>
#include <span>

namespace rr::sim {

struct RacerStanding {
    float progress = 0.0F;
    float distanceToNextGate = 0.0F;
    std::optional<int> finishPlace;
};

/// Fills `order`, one slot per racer, with racer indexes first place first: finishers by place,
/// then progress, then nearness to a gate, then racer index.
void RankRacers(std::span<const RacerStanding> racers, std::span<std::size_t> order);

}
