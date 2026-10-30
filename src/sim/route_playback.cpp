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
    StartSegment(0, {.position = route_.startPosition, .rotation = route_.startRotation}, 0);
    contactCount_ = kAllWheels;
}

void RoutePlayback::Advance(Milliseconds elapsed, float speed) {
    RR_ASSERT(speed >= 0.0F);
    timeMs_ += speed * static_cast<float>(elapsed.count());
    auto at = static_cast<std::int32_t>(timeMs_);
    while (at >= SegmentEndMs()) {
        const std::int32_t endMs = SegmentEndMs();
        if (target_ + 1 < route_.points.size()) {
            StartSegment(target_ + 1, to_, endMs);
            continue;
        }
        at += route_.loopTimeMs - endMs;
        timeMs_ += static_cast<float>(route_.loopTimeMs - endMs);
        StartSegment(route_.loopPoint + 1,
                     {.position = route_.loopPosition, .rotation = route_.loopRotation},
                     route_.loopTimeMs);
    }
}

Transform RoutePlayback::Pose() const {
    const auto at = static_cast<std::int32_t>(timeMs_);
    const auto alpha = static_cast<float>(at - segmentStartMs_) /
                       static_cast<float>(route_.points[target_].durationMs);
    return Interpolate(from_, to_, alpha);
}

void RoutePlayback::StartSegment(std::size_t target, Transform from, std::int32_t startMs) {
    const assets::RoutePoint& point = route_.points[target];
    target_ = target;
    to_ = {.position = from.position + point.offset, .rotation = point.rotation};
    from_ = from;
    segmentStartMs_ = startMs;
    contactCount_ = ContactCountOf(point);
}

std::int32_t RoutePlayback::SegmentEndMs() const {
    return segmentStartMs_ + route_.points[target_].durationMs;
}

}
