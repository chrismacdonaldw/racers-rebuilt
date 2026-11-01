#include "app/trigger_spheres.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr int kRingAxes = 3;
constexpr int kRingSegments = 24;
constexpr auto kLinesPerSphere =
    static_cast<std::size_t>(kRingAxes) * static_cast<std::size_t>(kRingSegments);
constexpr render::Color kRacerColor{.r = 0.3F, .g = 0.5F, .b = 0.9F};
constexpr render::Color kEnvironmentColor{.r = 0.95F, .g = 0.6F, .b = 0.2F};
constexpr render::Color kInsideColor{.r = 0.2F, .g = 0.9F, .b = 0.3F};

Vector3 RingPoint(const assets::TriggerSphere& sphere, int axis, int segment) {
    const float angle =
        2.0F * std::numbers::pi_v<float> * static_cast<float>(segment) / kRingSegments;
    const float a = sphere.radius * std::cos(angle);
    const float b = sphere.radius * std::sin(angle);
    const std::array offsets{Vector3{.x = a, .y = b}, Vector3{.x = a, .z = b},
                             Vector3{.y = a, .z = b}};
    return sphere.centre + offsets.at(static_cast<std::size_t>(axis));
}

void AddSphere(const assets::TriggerSphere& sphere, std::vector<render::DebugLine>& out) {
    for (int axis = 0; axis < kRingAxes; ++axis) {
        for (int segment = 0; segment < kRingSegments; ++segment) {
            out.push_back({.from = ToEngine(RingPoint(sphere, axis, segment)),
                           .to = ToEngine(RingPoint(sphere, axis, segment + 1))});
        }
    }
}

void Colour(std::span<render::DebugLine> lines, std::size_t sphere, render::Color color) {
    std::ranges::for_each(lines.subspan(sphere * kLinesPerSphere, kLinesPerSphere),
                          [&](render::DebugLine& line) { line.color = color; });
}

}

TriggerRings::TriggerRings(const sim::TrackEvents& triggers) {
    const std::span<const assets::TriggerSphere> racer = triggers.RacerSpheres().Spheres();
    const std::span<const assets::TriggerSphere> environment =
        triggers.EnvironmentSpheres().Spheres();
    lines_.reserve((racer.size() + environment.size()) * kLinesPerSphere);
    for (const assets::TriggerSphere& sphere : racer) {
        AddSphere(sphere, lines_);
    }
    for (const assets::TriggerSphere& sphere : environment) {
        AddSphere(sphere, lines_);
    }
    Light(triggers);
}

void TriggerRings::Light(const sim::TrackEvents& triggers) {
    const sim::RacerTriggers& racer = triggers.RacerSpheres();
    for (std::size_t i = 0; i < racer.Spheres().size(); ++i) {
        Colour(lines_, i, racer.Inside(i) != 0 ? kInsideColor : kRacerColor);
    }
    const sim::EnvironmentTriggers& environment = triggers.EnvironmentSpheres();
    const std::size_t first = racer.Spheres().size();
    for (std::size_t i = 0; i < environment.Spheres().size(); ++i) {
        Colour(lines_, first + i, environment.IsActive(i) ? kInsideColor : kEnvironmentColor);
    }
}

}
