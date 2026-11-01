#pragma once

#include <cstddef>
#include <optional>

#include "assets/event_id.hpp"
#include "core/bounding_sphere.hpp"
#include "core/duration.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::sim {

inline constexpr assets::EventId kGhostTrigger = 10;
inline constexpr std::size_t kGhostBodyNode = 1;

struct Ghost {
    std::optional<std::size_t> entity;
    BoundingSphere reach;
    Milliseconds nearWait{0};

    void Tick(HazardContext& context);
    void Touch(HazardContext& context) const;
};

[[nodiscard]] Ghost LoadGhost(const TrackEntities& entities);

}
