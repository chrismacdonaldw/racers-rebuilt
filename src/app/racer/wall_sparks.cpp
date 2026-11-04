#include "app/racer/wall_sparks.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>

#include "app/race_fx.hpp"
#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr float kMinScrapeSpeed = 0.03F;
constexpr float kCarHalfWidth = 2.5F;
constexpr Vector3 kSparkLift{.z = 1.0F};
constexpr Milliseconds kSparkGap{60};
constexpr float kVelocityScale = 1000.0F;

}

void WallSparks::Tick(std::span<const sim::Racer> racers, render::FxSystem& fx,
                      Milliseconds elapsed) {
    const std::optional<render::FxEffectId> sparks = fx.EffectFor(kWallSparksEffect);
    cooldowns_.resize(racers.size());
    for (std::size_t i = 0; i < racers.size() && sparks; ++i) {
        Milliseconds& cooldown = cooldowns_[i];
        cooldown = std::max(cooldown - elapsed, Milliseconds{0});
        const sim::BodyState& body = racers[i].Body();
        if (!body.hasWallContact || cooldown > Milliseconds{0} ||
            Length(body.motion.velocity) < kMinScrapeSpeed) {
            continue;
        }
        cooldown = kSparkGap;
        const Vector3 contact =
            body.motion.position - (body.wallNormal * kCarHalfWidth) + kSparkLift;
        fx.Burst(*sparks, {.position = ToEngine(contact),
                           .velocity = ToEngine(body.motion.velocity) * kVelocityScale});
    }
}

}
