#include "sim/route_playback.hpp"

#include <utility>

#include "core/assert.hpp"
#include "core/quaternion.hpp"

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
    Move(cursor_, speed * static_cast<float>(elapsed.count()));
}

void RoutePlayback::Seek(const Vector3& move) {
    const Vector3 segment = cursor_.to.position - cursor_.from.position;
    const float length = Length(segment);
    if (length > 0.0F) {
        const auto durationMs = static_cast<float>(route_.points[cursor_.target].durationMs);
        Move(cursor_, Dot(move, segment) * durationMs / (length * length));
    }
}

Transform RoutePlayback::PoseAhead(float routeMs) const {
    RR_ASSERT(routeMs >= 0.0F);
    Cursor ahead = cursor_;
    Move(ahead, routeMs);
    return PoseAt(ahead);
}

void RoutePlayback::Move(Cursor& cursor, float routeMs) const {
    cursor.timeMs += routeMs;
    if (routeMs < 0.0F) {
        MoveBack(cursor);
        return;
    }
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

void RoutePlayback::MoveBack(Cursor& cursor) const {
    while (static_cast<std::int32_t>(cursor.timeMs) < cursor.segmentStartMs) {
        if (cursor.target == 0) {
            cursor.timeMs = 0.0F;
            return;
        }
        const std::size_t target = cursor.target - 1;
        const assets::RoutePoint& point = route_.points[target];
        const Quaternion rotation =
            target == 0 ? route_.startRotation : route_.points[target - 1].rotation;
        StartSegment(cursor, target,
                     {.position = cursor.from.position - point.offset, .rotation = rotation},
                     cursor.segmentStartMs - point.durationMs);
    }
}

Transform RoutePlayback::PoseAt(const Cursor& cursor) const {
    return Interpolate(cursor.from, cursor.to, AlphaAt(cursor));
}

float RoutePlayback::AlphaAt(const Cursor& cursor) const {
    const auto at = static_cast<std::int32_t>(cursor.timeMs);
    return static_cast<float>(at - cursor.segmentStartMs) /
           static_cast<float>(route_.points[cursor.target].durationMs);
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
