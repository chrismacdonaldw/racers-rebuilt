#include "app/racer/wheel_spray.hpp"

#include <string>

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr float kMinSpeed = 0.008F;
constexpr float kVelocityScale = 0.6F * 1000.0F;
constexpr std::array<std::size_t, 2> kRearWheels{2, 3};

}

bool IsKickingUp(const sim::Racer& racer) {
    const sim::BodyState& body = racer.Body();
    return !body.hover.isHovering && !racer.IsSpunOut() && !racer.Status().isGhost &&
           body.derived.forwardSpeed > kMinSpeed;
}

std::optional<std::string_view> WheelParticleOf(const sim::WheelProbe& wheel,
                                                std::span<const assets::SurfaceEntry> surfaces) {
    const std::optional<std::size_t> entry = wheel.surface.tableEntry;
    if (!wheel.isInContact || !entry || *entry >= surfaces.size()) {
        return std::nullopt;
    }
    const std::optional<std::string>& particle = surfaces[*entry].particle;
    return particle ? std::optional<std::string_view>(*particle) : std::nullopt;
}

void WheelSpray::Update(std::size_t slot, const sim::Racer& racer, render::FxSystem& fx,
                        float elapsedMs) {
    if (slot >= cars_.size()) {
        cars_.resize(slot + 1);
    }
    const bool isKicking = IsKickingUp(racer);
    const Vector3 velocity = ToEngine(racer.Body().motion.velocity) * kVelocityScale;
    for (std::size_t i = 0; i < kRearWheels.size(); ++i) {
        const sim::WheelProbe& probe = racer.Body().motion.wheels.at(kRearWheels.at(i));
        const std::optional<std::string_view> name =
            isKicking ? WheelParticleOf(probe, surfaces_) : std::nullopt;
        const std::optional<render::FxEffectId> effect = name ? fx.EffectFor(*name) : std::nullopt;
        Wheel& wheel = cars_[slot].at(i);
        if (effect != wheel.effect) {
            wheel = {.effect = effect};
        }
        if (effect) {
            fx.Emit(*effect, {.position = ToEngine(probe.position), .velocity = velocity},
                    elapsedMs, wheel.emission);
        }
    }
}

}
