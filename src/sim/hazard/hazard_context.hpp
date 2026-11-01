#pragma once

#include <span>

#include "core/bounding_sphere.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/racer_contacts.hpp"
#include "sim/track/track_entities.hpp"
#include "sim/track/track_events.hpp"

namespace rr::sim {

/// Rebuilt per use, since the session it points into can move. `bodies` is set only for touches.
struct HazardContext {
    std::span<Racer> racers;
    std::span<const BoundingSphere> reaches;
    std::span<const ContactBody> bodies;
    const TrackEntities* entities = nullptr;
    TrackEvents* events = nullptr;
};

}
