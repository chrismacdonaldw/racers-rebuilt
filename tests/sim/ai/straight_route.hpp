#pragma once

#include <cstddef>
#include <vector>

#include "assets/race_route.hpp"
#include "core/vector3.hpp"

namespace rr::test {

/// Runs along +X from `start`, one unit every 100 ms, facing the way it goes.
[[nodiscard]] inline assets::RaceRoute StraightRoute(const Vector3& start,
                                                     std::size_t pointCount = 300) {
    const assets::RoutePoint point{.offset = {.x = 1.0F}, .kind = 3, .durationMs = 100};
    return {.startPosition = start,
            .loopPosition = start + point.offset,
            .loopTimeMs = point.durationMs,
            .loopPoint = 0,
            .points = std::vector(pointCount, point)};
}

}
