#pragma once

#include <cstddef>
#include <optional>

#include "assets/event_id.hpp"
#include "core/bounding_sphere.hpp"
#include "core/duration.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::sim {

inline constexpr assets::EventId kLavaBlobTrigger = 0;
inline constexpr assets::EventId kLavaBlobEvent = 43;

struct LavaBlob {
    std::optional<std::size_t> entity;
    BoundingSphere reach;
    Milliseconds eventWait{0};

    void Activate() { eventWait = Milliseconds{0}; }
    void Tick(HazardContext& context);
    void Touch(HazardContext& context) const;
};

[[nodiscard]] LavaBlob LoadLavaBlob(const TrackEntities& entities);

}
