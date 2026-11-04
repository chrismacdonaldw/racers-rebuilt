#include "sim/ai/gate_path.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "core/quaternion.hpp"

namespace rr::sim {
namespace {

constexpr float kSpacing = 10.0F;
constexpr float kRideHeight = 0.2F;
constexpr float kClimb = 12.0F;
constexpr float kFall = 80.0F;
constexpr float kBandAbove = 5.0F;
constexpr float kBandBelow = 60.0F;
constexpr float kFloorNormal = 0.5F;
constexpr int kFloorTries = 4;
constexpr float kBelowHit = 0.01F;
constexpr float kReach = 30.0F;
constexpr float kRouteWidth = 8.0F;
constexpr int kReachSteps = 10;
constexpr float kStepUp = 3.0F;
constexpr float kStepDown = 4.0F;
constexpr float kWallLift = 2.0F;
constexpr int kCentringRounds = 4;
constexpr int kSmoothingPasses = 20;
constexpr float kJoinAhead = 30.0F;
constexpr float kNominalSpeed = 100.0F;
constexpr float kMsPerSecond = 1000.0F;
constexpr std::uint8_t kGroundKind = 3;
constexpr std::uint8_t kAirKind = 0;
constexpr std::size_t kFewestGates = 2;
constexpr float kMostPoints = 5000.0F;

Vector3 Flat(const Vector3& v) {
    return {.x = v.x, .y = v.y};
}

Vector3 LeftOf(const Vector3& forward) {
    return {.x = -forward.y, .y = forward.x};
}

Quaternion Heading(const Vector3& forward) {
    return FromAxisAngle({.z = 1.0F}, std::atan2(forward.y, forward.x));
}

std::optional<float> FloorHeight(const CollisionQuery& world, const Vector3& at, float above,
                                 float below) {
    Vector3 from = at + Vector3{.z = above};
    const Vector3 to = at - Vector3{.z = below};
    for (int i = 0; i < kFloorTries; ++i) {
        const std::optional<SegmentHit> hit = CastSolid(world, from, to);
        if (!hit) {
            return std::nullopt;
        }
        if (std::abs(hit->normal.z) > kFloorNormal) {
            return hit->point.z;
        }
        from = hit->point - Vector3{.z = kBelowHit};
    }
    return std::nullopt;
}

std::optional<float> FloorNear(const CollisionQuery& world, const Vector3& point,
                               std::optional<float> previous) {
    if (previous) {
        const Vector3 level{.x = point.x, .y = point.y, .z = *previous};
        if (const std::optional<float> floor = FloorHeight(world, level, kClimb, kFall)) {
            return floor;
        }
    }
    return FloorHeight(world, point, kBandAbove, kBandBelow);
}

float ClearReach(const CollisionQuery& world, const Vector3& ground, const Vector3& side) {
    const float step = kReach / static_cast<float>(kReachSteps);
    Vector3 from = ground + Vector3{.z = kWallLift};
    float floor = ground.z;
    float clear = 0.0F;
    for (int i = 1; i <= kReachSteps; ++i) {
        const float distance = step * static_cast<float>(i);
        const Vector3 at = ground + (side * distance);
        const std::optional<float> next =
            FloorHeight(world, {.x = at.x, .y = at.y, .z = floor}, kStepUp, kStepDown);
        if (!next) {
            break;
        }
        const Vector3 to{.x = at.x, .y = at.y, .z = *next + kWallLift};
        if (CastSolid(world, from, to)) {
            break;
        }
        clear = distance;
        from = to;
        floor = *next;
    }
    return clear;
}

std::optional<std::vector<std::size_t>> LoopOrder(std::span<const assets::Checkpoint> gates) {
    if (gates.size() < kFewestGates) {
        return std::nullopt;
    }
    std::vector<std::size_t> order(1, 0);
    std::size_t next = gates.front().links[0];
    while (next != 0) {
        if (next >= gates.size() || order.size() == gates.size()) {
            return std::nullopt;
        }
        order.push_back(next);
        next = gates[next].links[0];
    }
    return order;
}

float SegmentsBetween(const Vector3& from, const Vector3& to) {
    const float segments = Length(Flat(to - from)) / kSpacing;
    return std::isfinite(segments) && std::isfinite(from.z) && std::isfinite(to.z)
               ? segments
               : std::numeric_limits<float>::infinity();
}

void AppendLine(std::vector<Vector3>& points, const Vector3& from, const Vector3& to) {
    const float segments = std::clamp(SegmentsBetween(from, to), 1.0F, kMostPoints);
    const auto count = static_cast<int>(std::lround(segments));
    for (int i = 0; i < count; ++i) {
        points.push_back(Lerp(from, to, static_cast<float>(i) / static_cast<float>(count)));
    }
}

Vector3 LeftAt(std::span<const Vector3> points, std::size_t i, bool isClosed) {
    const std::size_t last = points.size() - 1;
    std::size_t before = i > 0 ? i - 1 : 0;
    std::size_t after = i < last ? i + 1 : last;
    if (isClosed) {
        before = i > 0 ? i - 1 : last;
        after = i < last ? i + 1 : 0;
    }
    return LeftOf(Normalized(Flat(points[after] - points[before]), {.x = 1.0F}));
}

std::vector<GatePoint> ProbeCorridor(const CollisionQuery& world, std::span<const Vector3> points,
                                     bool isClosed, std::optional<float> previous) {
    std::vector<GatePoint> corridor;
    corridor.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Vector3& point = points[i];
        const std::optional<float> floor = FloorNear(world, point, previous);
        if (!floor) {
            corridor.push_back({.position = {.x = point.x,
                                             .y = point.y,
                                             .z = previous.value_or(point.z) + kRideHeight},
                                .isAirborne = true});
            continue;
        }
        previous = floor;
        const Vector3 ground{.x = point.x, .y = point.y, .z = *floor};
        const Vector3 left = LeftAt(points, i, isClosed);
        corridor.push_back({.position = ground + Vector3{.z = kRideHeight},
                            .widthLeft = ClearReach(world, ground, left),
                            .widthRight = ClearReach(world, ground, left * -1.0F)});
    }
    return corridor;
}

void Smooth(std::vector<Vector3>& points) {
    const std::size_t count = points.size();
    std::vector<Vector3> smoothed = points;
    for (int pass = 0; pass < kSmoothingPasses; ++pass) {
        for (std::size_t i = 0; i < count; ++i) {
            const Vector3& before = points[(i + count - 1) % count];
            const Vector3& after = points[(i + 1) % count];
            smoothed[i].x = (before.x + (2.0F * points[i].x) + after.x) / 4.0F;
            smoothed[i].y = (before.y + (2.0F * points[i].y) + after.y) / 4.0F;
        }
        std::swap(points, smoothed);
    }
}

std::size_t JoinPoint(std::span<const GatePoint> loop, const Vector3& start,
                      const Vector3& forward) {
    const std::size_t count = loop.size();
    std::size_t join = 0;
    float nearest = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < count; ++i) {
        const Vector3 gap = Flat(loop[i].position - start);
        const Vector3 along =
            loop[(i + 1) % count].position - loop[(i + count - 1) % count].position;
        const float distance = Length(gap);
        if (Dot(gap, forward) >= kJoinAhead && Dot(Flat(along), forward) > 0.0F &&
            distance < nearest) {
            join = i;
            nearest = distance;
        }
    }
    return join;
}

std::int32_t DurationOf(const Vector3& offset) {
    return std::max<std::int32_t>(1, static_cast<std::int32_t>(std::lround(
                                         Length(Flat(offset)) * kMsPerSecond / kNominalSpeed)));
}

}

std::optional<GateLoop> GateLoop::Trace(std::span<const assets::Checkpoint> gates,
                                        const CollisionQuery& world) {
    const std::optional<std::vector<std::size_t>> order = LoopOrder(gates);
    if (!order) {
        return std::nullopt;
    }
    float segments = 0.0F;
    for (std::size_t i = 0; i < order->size(); ++i) {
        segments += SegmentsBetween(gates[(*order)[i]].position,
                                    gates[(*order)[(i + 1) % order->size()]].position);
    }
    if (!(segments <= kMostPoints)) {
        return std::nullopt;
    }
    std::vector<Vector3> points;
    for (std::size_t i = 0; i < order->size(); ++i) {
        AppendLine(points, gates[(*order)[i]].position,
                   gates[(*order)[(i + 1) % order->size()]].position);
    }
    for (int round = 0; round < kCentringRounds; ++round) {
        const std::vector<GatePoint> corridor = ProbeCorridor(world, points, true, std::nullopt);
        for (std::size_t i = 0; i < points.size(); ++i) {
            const GatePoint& point = corridor[i];
            if (!point.isAirborne) {
                const float shift = (point.widthLeft - point.widthRight) / 2.0F;
                points[i] += LeftAt(points, i, true) * shift;
            }
        }
        Smooth(points);
    }
    return GateLoop(ProbeCorridor(world, points, true, std::nullopt));
}

assets::RaceRoute GateLoop::RouteFrom(const RacerStart& start, const CollisionQuery& world) const {
    const Vector3 forward{.x = std::cos(start.headingRadians), .y = std::sin(start.headingRadians)};
    const std::size_t count = points_.size();
    const std::size_t join = JoinPoint(points_, start.position, forward);
    std::vector<Vector3> approach;
    AppendLine(approach, start.position, points_[join].position);
    approach.erase(approach.begin());
    std::vector<GatePoint> path = ProbeCorridor(world, approach, false, start.position.z);
    const std::size_t loopPoint = path.size();
    for (std::size_t i = 0; i <= count; ++i) {
        path.push_back(points_[(join + i) % count]);
    }
    assets::RaceRoute route{.startPosition = start.position,
                            .startRotation = Heading(forward),
                            .loopPosition = path[loopPoint].position,
                            .loopPoint = loopPoint};
    Vector3 previous = start.position;
    std::int32_t timeMs = 0;
    for (std::size_t i = 0; i < path.size(); ++i) {
        const GatePoint& point = path[i];
        const Vector3& next = path[i + 1 < path.size() ? i + 1 : loopPoint + 1].position;
        const Vector3 offset = point.position - previous;
        const std::int32_t durationMs = DurationOf(offset);
        route.points.push_back({.offset = offset,
                                .rotation = Heading(Normalized(Flat(next - previous), forward)),
                                .widthLeft = std::min(point.widthLeft, kRouteWidth),
                                .widthRight = std::min(point.widthRight, kRouteWidth),
                                .kind = point.isAirborne ? kAirKind : kGroundKind,
                                .durationMs = durationMs});
        timeMs += durationMs;
        if (i == loopPoint) {
            route.loopTimeMs = timeMs;
            route.loopRotation = route.points.back().rotation;
        }
        previous = point.position;
    }
    return route;
}

}
