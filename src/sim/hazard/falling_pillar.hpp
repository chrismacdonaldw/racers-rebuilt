#pragma once

#include <cstddef>
#include <optional>

#include "assets/event_id.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::sim {

inline constexpr assets::EventId kFallingPillarTrigger = 10;
inline constexpr assets::EventId kPillarLandedEvent = 7;

struct FallingPillar {
    static constexpr bool kIgnoresEnd = true;

    std::optional<std::size_t> entity;
    bool hasLanded = false;

    void Activate(HazardContext& context);
    void Tick(HazardContext& context);
};

/// Freezes the pillar until its trigger.
[[nodiscard]] FallingPillar LoadFallingPillar(TrackEntities& entities);

}
