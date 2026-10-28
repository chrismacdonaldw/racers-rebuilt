#include "sim/route_playback.hpp"

#include <utility>

namespace rr::sim {
namespace {

Milliseconds DurationOf(const assets::RoutePoint& point) {
    return Milliseconds{point.durationMs};
}

}

RoutePlayback::RoutePlayback(assets::RaceRoute route) : route_(std::move(route)) {
    StartSegment(0, {.position = route_.startPosition, .rotation = route_.startRotation});
}

void RoutePlayback::Advance(Milliseconds elapsed) {
    intoSegment_ += elapsed;
    while (intoSegment_ >= DurationOf(route_.points[target_])) {
        intoSegment_ -= DurationOf(route_.points[target_]);
        if (target_ + 1 < route_.points.size()) {
            StartSegment(target_ + 1, to_);
        } else {
            StartSegment(route_.loopPoint + 1,
                         {.position = route_.loopPosition, .rotation = route_.loopRotation});
        }
    }
}

Transform RoutePlayback::Pose() const {
    const auto alpha = static_cast<float>(intoSegment_.count()) /
                       static_cast<float>(DurationOf(route_.points[target_]).count());
    return Interpolate(from_, to_, alpha);
}

void RoutePlayback::StartSegment(std::size_t target, Transform from) {
    const assets::RoutePoint& point = route_.points[target];
    target_ = target;
    to_ = {.position = from.position + point.offset, .rotation = point.rotation};
    from_ = from;
}

}
