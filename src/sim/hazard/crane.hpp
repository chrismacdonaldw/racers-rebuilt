#pragma once

#include <cstddef>
#include <optional>

#include "assets/event_id.hpp"
#include "core/vector3.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::sim {

inline constexpr assets::EventId kCraneTrigger = 1;
inline constexpr assets::EventId kCraneSwingEvent = 20;
inline constexpr assets::EventId kCraneHitEvent = 21;
inline constexpr std::size_t kCraneBallNode = 3;

struct Crane {
    std::optional<std::size_t> entity;
    bool isArmed = false;
    Vector3 ball;

    void Activate() { isArmed = true; }
    void Tick(HazardContext& context);
    void Touch(HazardContext& context) const;
};

[[nodiscard]] Crane LoadCrane(const TrackEntities& entities);

}
