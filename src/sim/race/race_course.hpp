#pragma once

#include <span>

#include "assets/brick_layout.hpp"
#include "assets/hazard_list.hpp"
#include "assets/target_points.hpp"
#include "sim/ai/route_choice.hpp"
#include "sim/collision/track_collision.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/racer.hpp"
#include "sim/race/checkpoint_graph.hpp"
#include "sim/track/track_entity_sources.hpp"
#include "sim/track/track_events.hpp"

namespace rr::sim {

/// What a session reads from its track. Everything it points to must outlive the session.
struct RaceCourse {
    const CheckpointGraph* checkpoints = nullptr;
    std::span<const RacerStart> grid;
    const CollisionQuery* world = nullptr;
    float lowestTrackZ = 0.0F;
    const GridRoutes* routes = nullptr;
    const assets::BrickLayout* bricks = nullptr;
    std::span<const assets::TargetPoint> targets;
    const TrackEventSources* events = nullptr;
    const TrackCollision* collision = nullptr;
    const TrackEntitySources* entities = nullptr;
    std::span<const assets::Hazard> hazards;
    bool isMirrored = false;
};

}
