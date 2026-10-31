#include "sim/route_playback.hpp"

#include <utility>

#include "core/assert.hpp"

namespace rr::sim {
namespace {

constexpr int kAllWheels = 4;
constexpr std::uint8_t kAllWheelsKind = 3;

int ContactCountOf(const assets::RoutePoint& point) {
    return point.kind >= kAllWheelsKind ? kAllWheels : point.kind;
}

}

RoutePlayback::RoutePlayback(assets::RaceRoute route) : route_(std::move(route)) {
    StartSegment(cursor_, 0, {.position = route_.startPosition, .rotation = route_.startRotation},
                 0);
    cursor_.contactCount = kAllWheels;
}

void RoutePlayback::Advance(Milliseconds elapsed, float speed) {
    RR_ASSERT(speed >= 0.0F);
    Move(cursor_, speed * static_cast<float>(elapsed.count()));
}

Transform RoutePlayback::PoseAhead(float routeMs) const {
    RR_ASSERT(routeMs >= 0.0F);
    Cursor ahead = cursor_;
    Move(ahead, routeMs);
    return PoseAt(ahead);
}

void RoutePlayback::Move(Cursor& cursor, float routeMs) const {
    cursor.timeMs += routeMs;
    auto at = static_cast<std::int32_t>(cursor.timeMs);
    while (at >= SegmentEndMs(cursor)) {
        const std::int32_t endMs = SegmentEndMs(cursor);
        if (cursor.target + 1 < route_.points.size()) {
            StartSegment(cursor, cursor.target + 1, cursor.to, endMs);
            continue;
        }
        at += route_.loopTimeMs - endMs;
        cursor.timeMs += static_cast<float>(route_.loopTimeMs - endMs);
        StartSegment(cursor, route_.loopPoint + 1,
                     {.position = route_.loopPosition, .rotation = route_.loopRotation},
                     route_.loopTimeMs);
    }
}

Transform RoutePlayback::PoseAt(const Cursor& cursor) const {
    const auto at = static_cast<std::int32_t>(cursor.timeMs);
    const auto alpha = static_cast<float>(at - cursor.segmentStartMs) /
                       static_cast<float>(route_.points[cursor.target].durationMs);
    return Interpolate(cursor.from, cursor.to, alpha);
}

void RoutePlayback::StartSegment(Cursor& cursor, std::size_t target, Transform from,
                                 std::int32_t startMs) const {
    const assets::RoutePoint& point = route_.points[target];
    cursor.target = target;
    cursor.to = {.position = from.position + point.offset, .rotation = point.rotation};
    cursor.from = from;
    cursor.segmentStartMs = startMs;
    cursor.contactCount = ContactCountOf(point);
}

std::int32_t RoutePlayback::SegmentEndMs(const Cursor& cursor) const {
    return cursor.segmentStartMs + route_.points[cursor.target].durationMs;
}

}
