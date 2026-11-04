#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "assets/race_route.hpp"
#include "core/quaternion.hpp"
#include "core/vector3.hpp"

namespace rr::test {

/// Faces along each step, 50 ms apart, with the same width on both sides.
[[nodiscard]] inline assets::RaceRoute RouteThrough(const std::vector<Vector3>& positions,
                                                    float width) {
    assets::RaceRoute route{.startPosition = positions.front()};
    for (std::size_t i = 1; i < positions.size(); ++i) {
        const Vector3 step = positions[i] - positions[i - 1];
        route.points.push_back({.offset = step,
                                .rotation = FromAxisAngle({.z = 1.0F}, std::atan2(step.y, step.x)),
                                .widthLeft = width,
                                .widthRight = width,
                                .kind = 3,
                                .durationMs = 50});
    }
    route.loopPosition = route.startPosition + route.points.front().offset;
    route.loopRotation = route.points.front().rotation;
    route.loopTimeMs = route.points.front().durationMs;
    return route;
}

/// Along +X to the origin, a quarter turn left about (0, `radius`), then along +Y.
[[nodiscard]] inline std::vector<Vector3> LeftBend(float radius, float straight = 60.0F) {
    constexpr float kStep = 2.0F;
    std::vector<Vector3> positions;
    const auto straightSteps = static_cast<int>(std::ceil(straight / kStep));
    for (int i = straightSteps; i > 0; --i) {
        positions.push_back({.x = -kStep * static_cast<float>(i)});
    }
    const float quarter = std::numbers::pi_v<float> / 2.0F;
    const auto steps = static_cast<int>(std::ceil(radius * quarter / kStep));
    for (int i = 0; i <= steps; ++i) {
        const float angle = quarter * static_cast<float>(i) / static_cast<float>(steps);
        positions.push_back(
            {.x = radius * std::sin(angle), .y = radius * (1.0F - std::cos(angle))});
    }
    for (int i = 1; i <= straightSteps; ++i) {
        positions.push_back({.x = radius, .y = radius + (kStep * static_cast<float>(i))});
    }
    return positions;
}

}
