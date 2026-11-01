#pragma once

#include <cstddef>
#include <span>

#include "assets/event_id.hpp"
#include "core/duration.hpp"
#include "sim/physics/racer.hpp"
#include "sim/powerup/aim_cones.hpp"
#include "sim/race/random.hpp"

namespace rr::sim {

inline constexpr assets::EventId kTauntEvent = 999;
inline constexpr std::size_t kEventsPerTaunt = 2;
inline constexpr AimCone kTauntCone{.maxDistanceSquared = 169.0F, .cosine = 0.3F};
inline constexpr Milliseconds kTauntRetry{2000};

class AiTaunter {
public:
    /// True when `racer` taunts the nearest racer ahead of it this tick.
    [[nodiscard]] bool Tick(Milliseconds elapsed, std::span<const Racer> racers, std::size_t racer,
                            Random& random);

    [[nodiscard]] Milliseconds Wait() const { return wait_; }

private:
    Milliseconds wait_{0};
};

}
