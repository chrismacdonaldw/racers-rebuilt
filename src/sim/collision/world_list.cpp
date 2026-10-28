#include "sim/collision/world_list.hpp"

#include <algorithm>

#include "core/assert.hpp"

namespace rr::sim {

WorldList::WorldList(const CollisionWorld& track) {
    slots_[0] = &track;
}

bool WorldList::Add(const CollisionWorld& world) {
    const auto listed = std::span(slots_).first(count_);
    if (std::ranges::find(listed, &world) != listed.end()) {
        return true;
    }
    if (count_ == slots_.size()) {
        return false;
    }
    slots_.at(count_++) = &world;
    return true;
}

void WorldList::Remove(const CollisionWorld& world) {
    const auto extras = std::span(slots_).subspan(1, count_ - 1);
    const auto found = std::ranges::find(extras, &world);
    if (found == extras.end()) {
        return;
    }
    std::shift_left(found, extras.end(), 1);
    slots_.at(--count_) = nullptr;
}

std::optional<SegmentHit> WorldList::CastSegment(std::size_t world, const Vector3& from,
                                                 const Vector3& to) const {
    RR_ASSERT(world < count_);
    const std::size_t slot = world + 1 == count_ ? 0 : world + 1;
    return slots_.at(slot)->CastSegment(from, to);
}

}
