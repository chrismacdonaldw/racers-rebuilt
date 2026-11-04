#include "app/powerup/powerup_fx.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

#include "app/powerup/powerup_fx_effects.hpp"
#include "app/track_space.hpp"
#include "sim/physics/turbo.hpp"
#include "sim/powerup/brick_field.hpp"
#include "sim/powerup/warp_effect.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kExhaust{.x = -5.5F, .z = 2.5F};
constexpr Vector3 kFlameThrust{.x = -35.0F};
constexpr Vector3 kCarMiddle{.z = 2.0F};
constexpr float kPullSpeed = 70.0F;
constexpr float kPerSecond = 1000.0F;

void Keep(render::FxSystem& fx, std::optional<render::FxEffectId> effect, bool isOn,
          const render::EmitterPlacement& placement, float elapsedMs,
          render::FxEmission& emission) {
    if (!isOn || !effect) {
        emission = {};
        return;
    }
    fx.Emit(*effect, placement, elapsedMs, emission);
}

void BurstAt(render::FxSystem& fx, std::optional<render::FxEffectId> effect, const Vector3& at) {
    if (effect) {
        fx.Burst(*effect, {.position = ToEngine(at)});
    }
}

render::EmitterPlacement AtExhaust(const sim::Racer& racer) {
    const Transform pose = racer.Pose();
    const Vector3 velocity =
        (racer.Body().motion.velocity * kPerSecond) + Rotate(pose.rotation, kFlameThrust);
    return {.position = ToEngine(pose.position + Rotate(pose.rotation, kExhaust)),
            .velocity = ToEngine(velocity)};
}

bool IsWarpStarting(const sim::PowerupSystem& powerups, std::size_t racer) {
    return std::ranges::any_of(powerups.Active(), [&](std::uint16_t slot) {
        const sim::EffectSlot& effect = powerups.Slots()[slot];
        return effect.kind == sim::PowerupKind::Warp && effect.use.racer == racer &&
               effect.effect->Progress().phase ==
                   static_cast<std::uint8_t>(sim::WarpPhase::Starting);
    });
}

std::optional<Vector3> NearestMagnet(const sim::PowerupSystem& powerups, const Vector3& at) {
    std::optional<Vector3> nearest;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const std::uint16_t slot : powerups.Active()) {
        const sim::EffectSlot& effect = powerups.Slots()[slot];
        if (effect.kind != sim::PowerupKind::Magnet) {
            continue;
        }
        const Vector3 magnet = effect.effect->Pose().position;
        const float distance = Length(magnet - at);
        if (distance < nearestDistance) {
            nearest = magnet;
            nearestDistance = distance;
        }
    }
    return nearest;
}

}

PowerupFx::Effects PowerupFx::EffectsOf(const render::FxSystem& fx) {
    Effects effects{.flame = fx.EffectFor(kTurboFlameEffect),
                    .hover = fx.EffectFor(kHoverFlameEffect),
                    .swirl = fx.EffectFor(kWarpSwirlEffect),
                    .warpOut = fx.EffectFor(kWarpOutEffect),
                    .warpIn = fx.EffectFor(kWarpInEffect),
                    .pull = fx.EffectFor(kMagnetPullEffect),
                    .pulse = fx.EffectFor(kMagnetPulseEffect),
                    .white = fx.EffectFor(kWhitePickupEffect)};
    std::ranges::transform(kPickupEffects, effects.pickups.begin(),
                           [&](std::string_view name) { return fx.EffectFor(name); });
    return effects;
}

void PowerupFx::Tick(const sim::RaceSession& session, render::FxSystem& fx, Milliseconds elapsed) {
    const Effects& effects = effects_ ? *effects_ : effects_.emplace(EffectsOf(fx));
    racers_.resize(session.Racers().size());
    for (std::size_t slot = 0; slot < racers_.size(); ++slot) {
        TickRacer(slot, session, effects, fx, static_cast<float>(elapsed.count()));
    }
    BurstPickups(session.Bricks(), effects, fx);
}

void PowerupFx::BurstPickups(const sim::BrickField& bricks, const Effects& effects,
                             render::FxSystem& fx) {
    for (const sim::BrickEvent& event : bricks.Events()) {
        if (event.kind == sim::BrickEventKind::WhiteCollected) {
            BurstAt(fx, effects.white, bricks.White()[event.brick].brick.position);
        } else if (event.kind != sim::BrickEventKind::Respawned) {
            const sim::ColourBrick& brick = bricks.Coloured()[event.brick];
            BurstAt(fx, effects.pickups.at(static_cast<std::size_t>(brick.colour)),
                    brick.brick.position);
        }
    }
}

void PowerupFx::TickRacer(std::size_t slot, const sim::RaceSession& session, const Effects& effects,
                          render::FxSystem& fx, float elapsedMs) {
    const sim::Racer& racer = session.Racers()[slot];
    RacerEmissions& emissions = racers_[slot];
    const Vector3 middle = racer.Body().motion.position + kCarMiddle;
    const std::optional<sim::Turbo>& turbo = racer.ActiveTurbo();
    const bool isBoosting = turbo && turbo->phase == sim::TurboPhase::Boost;
    Keep(fx, isBoosting && turbo->level == sim::TurboLevel::Hover ? effects.hover : effects.flame,
         isBoosting, AtExhaust(racer), elapsedMs, emissions.flame);
    Keep(fx, effects.swirl, IsWarpStarting(session.Powerups(), slot),
         {.position = ToEngine(middle)}, elapsedMs, emissions.swirl);
    if (racer.Events().hasWarpEntered) {
        BurstAt(fx, effects.warpOut, middle);
    }
    if (racer.Events().hasWarpLanded) {
        BurstAt(fx, effects.warpIn, middle);
    }
    const std::optional<Vector3> magnet =
        racer.Status().isMagnetHeld ? NearestMagnet(session.Powerups(), middle) : std::nullopt;
    const Vector3 pull = magnet ? Normalized(*magnet - middle, {}) * kPullSpeed : Vector3{};
    Keep(fx, effects.pull, magnet.has_value(),
         {.position = ToEngine(middle), .velocity = ToEngine(pull)}, elapsedMs, emissions.pull);
    Keep(fx, effects.pulse, magnet.has_value(), {.position = ToEngine(middle)}, elapsedMs,
         emissions.pulse);
}

}
