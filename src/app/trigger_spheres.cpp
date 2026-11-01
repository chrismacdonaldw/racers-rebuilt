#include "app/trigger_spheres.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr int kRingSegments = 24;
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

void AddSphere(const assets::TriggerSphere& sphere, render::Color color,
               std::vector<render::DebugLine>& out) {
    for (int axis = 0; axis < 3; ++axis) {
        for (int segment = 0; segment < kRingSegments; ++segment) {
            out.push_back({.from = ToEngine(RingPoint(sphere, axis, segment)),
                           .to = ToEngine(RingPoint(sphere, axis, segment + 1)),
                           .color = color});
        }
    }
}

}

void DrawTriggerSpheres(const sim::TrackEvents& triggers, std::vector<render::DebugLine>& out) {
    const sim::RacerTriggers& racer = triggers.RacerSpheres();
    for (std::size_t i = 0; i < racer.Spheres().size(); ++i) {
        AddSphere(racer.Spheres()[i], racer.Inside(i) != 0 ? kInsideColor : kRacerColor, out);
    }
    const sim::EnvironmentTriggers& environment = triggers.EnvironmentSpheres();
    for (std::size_t i = 0; i < environment.Spheres().size(); ++i) {
        AddSphere(environment.Spheres()[i],
                  environment.IsActive(i) ? kInsideColor : kEnvironmentColor, out);
    }
}

}
