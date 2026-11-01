#pragma once

#include <cstddef>
#include <optional>

#include "assets/event_id.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::sim {

inline constexpr assets::EventId kHammerSwishTrigger = 50;
inline constexpr assets::EventId kHammerSwishEvent = 43;

struct HammerSwish {
    std::optional<std::size_t> entity;
    bool isArmed = false;

    void Activate() { isArmed = true; }
    void Tick(HazardContext& context);
};

[[nodiscard]] HammerSwish LoadHammerSwish(const TrackEntities& entities);

}
