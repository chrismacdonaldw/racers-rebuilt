#pragma once

#include <span>

#include "core/bounding_sphere.hpp"
#include "core/duration.hpp"
#include "sim/collision/track_surfaces.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/racer_contacts.hpp"
#include "sim/powerup/explosions.hpp"
#include "sim/race/random.hpp"
#include "sim/track/model_switches.hpp"
#include "sim/track/track_entities.hpp"
#include "sim/track/track_events.hpp"

namespace rr::sim {

/// Rebuilt per use, since the session it points into can move. `bodies` is set only for touches.
struct HazardContext {
    std::span<Racer> racers;
    std::span<const BoundingSphere> reaches;
    std::span<const ContactBody> bodies;
    TrackEntities* entities = nullptr;
    TrackEvents* events = nullptr;
    TrackSurfaces* surfaces = nullptr;
    ModelSwitches* models = nullptr;
    Random* random = nullptr;
    Milliseconds elapsed{0};
    const CollisionQuery* world = nullptr;
    Explosions* explosions = nullptr;
};

}
