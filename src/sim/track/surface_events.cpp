#include "sim/track/surface_events.hpp"

#include <algorithm>

namespace rr::sim {
namespace {

constexpr std::size_t kPaths = 2;
constexpr std::size_t kEdges = 2;

void FireBoth(TrackEvents& events, std::optional<std::int32_t> id, EventEdge edge,
              const Vector3& position, std::uint8_t racer) {
    if (!id) {
        return;
    }
    events.Fire({.id = *id, .edge = edge, .position = position});
    events.Fire({.id = *id, .edge = edge, .racer = racer});
}

WheelSurfaces SurfacesUnder(const BodyMotion& motion, const TrackSurfaces& surfaces) {
    WheelSurfaces under{};
    for (std::size_t i = 0; i < kWheelCount; ++i) {
        const WheelProbe& wheel = motion.wheels.at(i);
        const std::optional<std::size_t> entry = wheel.surface.tableEntry;
        if (wheel.isInContact && entry && *entry < surfaces.Count()) {
            under.at(i) = entry;
        }
    }
    return under;
}

bool IsFirst(const WheelSurfaces& under, std::size_t wheel) {
    for (std::size_t i = 0; i < wheel; ++i) {
        if (under.at(i) == under.at(wheel)) {
            return false;
        }
    }
    return true;
}

}

SurfaceEvents::SurfaceEvents(std::size_t racerCount) : under_(racerCount) {}

void SurfaceEvents::Hear(std::uint8_t racer, const Racer& car, const TrackSurfaces& surfaces,
                         TrackEvents& events) {
    for (const SurfaceTouch& touch : car.Events().touches.Touches()) {
        FireBoth(events, touch.event, EventEdge::Start, touch.point, racer);
        FireBoth(events, touch.event, EventEdge::End, touch.point, racer);
    }
    const BodyMotion& motion = car.Body().motion;
    const WheelSurfaces before = under_.at(racer);
    const WheelSurfaces now = SurfacesUnder(motion, surfaces);
    for (std::size_t i = 0; i < kWheelCount; ++i) {
        const std::optional<std::size_t> left = before.at(i);
        if (left && IsFirst(before, i) && std::ranges::find(now, left) == now.end()) {
            FireBoth(events, surfaces.Entry(*left).leaveEvent, EventEdge::End,
                     motion.wheels.at(i).position, racer);
        }
    }
    for (std::size_t i = 0; i < kWheelCount; ++i) {
        const std::optional<std::size_t> entered = now.at(i);
        if (entered && IsFirst(now, i) && std::ranges::find(before, entered) == before.end()) {
            FireBoth(events, surfaces.Entry(*entered).enterEvent, EventEdge::Start,
                     motion.wheels.at(i).position, racer);
        }
    }
    under_.at(racer) = now;
}

std::size_t SurfaceEvents::MostEventsPerRacer() {
    return (kMaxSurfaceTouches * kEdges * kPaths) + (kWheelCount * kEdges * kPaths);
}

}
