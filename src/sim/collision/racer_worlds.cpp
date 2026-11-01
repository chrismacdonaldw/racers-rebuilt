#include "sim/collision/racer_worlds.hpp"

#include <algorithm>

#include "core/assert.hpp"

namespace rr::sim {

void TriggerWorlds::SwitchOn(std::size_t world) {
    const auto listed = std::span(slots_).first(count_);
    const auto found = std::ranges::find(listed, world, &TriggerWorld::world);
    if (found != listed.end()) {
        found->left = kTriggerWorldTime;
    } else if (count_ < slots_.size()) {
        slots_.at(count_++) = {.world = world, .left = kTriggerWorldTime};
    }
}

void TriggerWorlds::Tick(Milliseconds elapsed) {
    const auto listed = std::span(slots_).first(count_);
    for (TriggerWorld& slot : listed) {
        slot.left -= elapsed;
    }
    const auto expired = std::ranges::remove_if(
        listed, [](const TriggerWorld& slot) { return slot.left <= Milliseconds{0}; });
    count_ -= expired.size();
}

RacerWorldView::RacerWorldView(const CollisionQuery& base, const TrackCollision* collision,
                               const TriggerWorlds& switched, const TrackSurfaces& surfaces)
    : base_(&base), surfaces_(&surfaces) {
    if (collision == nullptr) {
        return;
    }
    if (collision->checkpoints) {
        extras_.at(count_++) = &collision->worlds.at(*collision->checkpoints).world;
    }
    for (const TriggerWorld& slot : switched.Worlds()) {
        extras_.at(count_++) = &collision->worlds.at(slot.world).world;
    }
}

std::optional<SegmentHit> RacerWorldView::CastSegment(std::size_t world, const Vector3& from,
                                                      const Vector3& to) const {
    RR_ASSERT(world < WorldCount());
    std::optional<SegmentHit> hit = world < count_ ? extras_.at(world)->CastSegment(from, to)
                                                   : base_->CastSegment(world - count_, from, to);
    if (hit && hit->surface.tableEntry && *hit->surface.tableEntry < surfaces_->Count()) {
        hit->isSolid = surfaces_->IsSolid(*hit->surface.tableEntry);
    }
    return hit;
}

}
