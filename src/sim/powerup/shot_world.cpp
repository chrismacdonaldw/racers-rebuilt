#include "sim/powerup/shot_world.hpp"

#include "core/assert.hpp"

namespace rr::sim {

ShotWorld::ShotWorld(const CollisionQuery& world, const TrackSurfaces* surfaces,
                     TrackEvents* events)
    : world_(&world), surfaces_(surfaces), events_(events) {}

std::optional<SegmentHit> ShotWorld::CastSegment(std::size_t world, const Vector3& from,
                                                 const Vector3& to) const {
    RR_ASSERT(world == 0);
    std::optional<SegmentHit> nearest;
    for (std::size_t i = 0; i < world_->WorldCount(); ++i) {
        const std::optional<SegmentHit> hit = world_->CastSegment(i, from, to);
        if (hit && (!nearest || hit->fraction < nearest->fraction)) {
            nearest = hit;
        }
    }
    if (!nearest) {
        return std::nullopt;
    }
    SegmentHit hit = *nearest;
    const std::optional<std::size_t> entry = hit.surface.tableEntry;
    if (!entry || surfaces_ == nullptr || *entry >= surfaces_->Count()) {
        return hit;
    }
    const std::optional<std::int32_t> event = surfaces_->Entry(*entry).projectileEvent;
    if (event && events_ != nullptr) {
        events_->FireStartAndEnd(*event, hit.point);
    }
    hit.isSolid = !surfaces_->LetsShotsThrough(*entry);
    return hit;
}

ShotWorld ShotsOf(const PowerupContext& context) {
    return {*context.world, context.surfaces, context.trackEvents};
}

}
