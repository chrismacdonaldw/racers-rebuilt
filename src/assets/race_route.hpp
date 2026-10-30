#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/quaternion.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

/// `offset` moves from the previous point, or from the start for the first. `rotation` maps the
/// car's local axes into game space, with +X forward and +Z up.
struct RoutePoint {
    Vector3 offset;
    Quaternion rotation;
    float widthLeft = 0.0F;
    float widthRight = 0.0F;
    std::uint8_t kind = 0;
    std::int32_t durationMs = 0;
};

/// A recorded driving line in game space. After the last point it continues from `loopPoint`,
/// reached `loopTimeMs` after the start at `loopPosition`.
struct RaceRoute {
    Vector3 startPosition;
    Quaternion startRotation;
    Vector3 loopPosition;
    Quaternion loopRotation;
    std::int32_t loopTimeMs = 0;
    std::size_t loopPoint = 0;
    std::vector<RoutePoint> points;
};

[[nodiscard]] Result<RaceRoute> ParseRaceRoute(std::span<const std::byte> bytes);

/// The route for a mirrored track, reflected across the game's XZ plane.
[[nodiscard]] RaceRoute Mirrored(RaceRoute route);

}
