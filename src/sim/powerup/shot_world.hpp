#pragma once

#include <cstddef>
#include <optional>

#include "core/vector3.hpp"
#include "sim/collision/track_surfaces.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/powerup/powerup_effect.hpp"
#include "sim/track/track_events.hpp"

namespace rr::sim {

/// Shots see only the nearest hit, fire its projectile event, and pass shot-passable surfaces.
class ShotWorld final : public CollisionQuery {
public:
    ShotWorld(const CollisionQuery& world, const TrackSurfaces* surfaces, TrackEvents* events);

    [[nodiscard]] std::size_t WorldCount() const override { return 1; }

    [[nodiscard]] std::optional<SegmentHit> CastSegment(std::size_t world, const Vector3& from,
                                                        const Vector3& to) const override;

private:
    const CollisionQuery* world_;
    const TrackSurfaces* surfaces_;
    TrackEvents* events_;
};

[[nodiscard]] ShotWorld ShotsOf(const PowerupContext& context);

}
