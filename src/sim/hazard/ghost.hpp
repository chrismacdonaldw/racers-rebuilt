#pragma once

#include <cstddef>
#include <optional>

#include "assets/event_id.hpp"
#include "core/bounding_sphere.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::sim {

inline constexpr assets::EventId kGhostTrigger = 10;

struct Ghost {
    std::optional<std::size_t> entity;
    BoundingSphere reach;

    void Tick(HazardContext& context);
    void Touch(HazardContext& context) const;
};

[[nodiscard]] Ghost LoadGhost(const TrackEntities& entities);

}
