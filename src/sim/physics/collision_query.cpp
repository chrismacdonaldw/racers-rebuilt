#include "sim/physics/collision_query.hpp"

namespace rr::sim {

std::optional<SegmentHit> CastSolid(const CollisionQuery& world, const Vector3& from,
                                    const Vector3& to) {
    std::optional<SegmentHit> nearest;
    for (std::size_t i = 0; i < world.WorldCount(); ++i) {
        const std::optional<SegmentHit> hit = world.CastSegment(i, from, to);
        if (hit && hit->isSolid && (!nearest || hit->fraction < nearest->fraction)) {
            nearest = hit;
        }
    }
    return nearest;
}

}
