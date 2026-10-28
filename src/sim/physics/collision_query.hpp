#pragma once

#include <optional>

#include "core/vector3.hpp"
#include "sim/physics/surface.hpp"

namespace rr::sim {

struct SegmentHit {
    Vector3 point;
    Vector3 normal;
    float fraction = 0.0F;
    Surface surface;
    bool isSolid = true;
};

class CollisionQuery {
public:
    virtual ~CollisionQuery() = default;

    /// The first hit along the segment, solid or not.
    [[nodiscard]] virtual std::optional<SegmentHit> CastSegment(const Vector3& from,
                                                                const Vector3& to) const = 0;
};

}
