#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "core/bounding_sphere.hpp"
#include "core/duration.hpp"
#include "sim/collision/track_surfaces.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/racer_contacts.hpp"
#include "sim/powerup/explosions.hpp"
#include "sim/powerup/powerup_effect.hpp"
#include "sim/powerup/powerup_kind.hpp"
#include "sim/race/random.hpp"
#include "sim/track/model_switches.hpp"
#include "sim/track/track_entities.hpp"
#include "sim/track/track_events.hpp"

namespace rr::sim {

/// `soundId` counts from 3000, the first sound of the track's bank. `position` is in game space.
struct HazardSound {
    std::int32_t soundId = 0;
    Vector3 position;
};

/// Rebuilt per use, as the session can move. `bodies` is for touches; `aim` is the hazard's own.
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
    PowerupContext* effects = nullptr;
    HazardAim* aim = nullptr;
    std::vector<HazardSound>* sounds = nullptr;
    std::uint8_t slot = 0;
};

}
