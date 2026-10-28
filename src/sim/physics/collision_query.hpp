#pragma once

#include <cstddef>
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
    bool isTrackWorld = true;
};

/// Worlds are numbered in the order racers test them.
class CollisionQuery {
public:
    virtual ~CollisionQuery() = default;

    [[nodiscard]] virtual std::size_t WorldCount() const = 0;

    /// Non-solid hits are returned too.
    [[nodiscard]] virtual std::optional<SegmentHit> CastSegment(std::size_t world,
                                                                const Vector3& from,
                                                                const Vector3& to) const = 0;
};

}
