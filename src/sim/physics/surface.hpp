#pragma once

#include "core/vector3.hpp"

namespace rr::sim {

/// The defaults are the road, which has no entry in a track's surface table.
struct Surface {
    float rollingResistance = 0.0F;
    float supportThreshold = 0.5F;
    float friction = 0.25F;
    float lateralGrip = 3.0F;
    Vector3 force;

    friend constexpr bool operator==(const Surface&, const Surface&) = default;
};

}
