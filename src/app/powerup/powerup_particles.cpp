#include "app/powerup/powerup_particles.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string_view>

#include "app/track_space.hpp"
#include "sim/physics/turbo.hpp"
#include "sim/powerup/dropped_trap.hpp"
#include "sim/powerup/dynamite_effect.hpp"
#include "sim/powerup/grapple_effect.hpp"
#include "sim/powerup/weapon_hits.hpp"
#include "sim/race/race_sounds.hpp"

namespace rr::app {
namespace {

using sim::PowerupKind;

constexpr std::string_view kShotSmoke = "cannsmk";
constexpr std::string_view kTurboSmoke = "trbsmke";
constexpr std::string_view kLightningHit = "lghthit";
constexpr std::string_view kOilBubbles = "oilbub";
constexpr std::string_view kDynamiteSparks = "dynsprk";

constexpr Vector3 kShotSmokeOffset{.z = 4.0F};
constexpr std::size_t kShotSmokePuffs = 3;
constexpr Vector3 kTurboSmokeOffset{.x = -2.0F, .z = 3.0F};
constexpr float kTurboSmokeVelocityScale = 0.7F;
constexpr std::uint8_t kSmokyTurboLevel = 2;
constexpr Milliseconds kTurboSmokeWindow{800};
constexpr Vector3 kFuseOffset{.x = -0.727682F, .y = 0.155761F, .z = 5.178523F};
constexpr Vector3 kBoltSource{.z = 3.0F};
constexpr Vector3 kGameUp{.z = 1.0F};
constexpr float kBubbleRadius = 6.0F;
constexpr float kPerSecond = 1000.0F;

bool Is(std::uint8_t phase, auto expected) {
    return phase == static_cast<std::uint8_t>(expected);
}

render::EmitterPlacement AtCar(const sim::Racer& racer, const Vector3& offset,
                               float velocityScale) {
    const Transform pose = racer.Pose();
    return {.position = ToEngine(pose.position + Rotate(pose.rotation, offset)),
            .rotation = TransformToEngine(pose).rotation,
            .velocity = ToEngine(racer.Body().motion.velocity) * (velocityScale * kPerSecond)};
}

render::EmitterPlacement AtFuse(const Transform& dynamite) {
    return {.position = ToEngine(dynamite.position + Rotate(dynamite.rotation, kFuseOffset)),
            .rotation = TransformToEngine(dynamite).rotation};
}

render::EmitterPlacement AtLauncher(const sim::EffectSlot& slot, const sim::RaceSession& session) {
    if (slot.use.hazard) {
        return {.position = ToEngine(session.TrackHazards().Aims()[*slot.use.hazard].source)};
    }
    return AtCar(session.Racers()[slot.use.racer], kShotSmokeOffset, 1.0F);
}

render::EmitterPlacement AtBoltEnd(const sim::EffectSlot& slot, const sim::RaceSession& session) {
    const Vector3 end = slot.effect->Pose().position;
    const Vector3 source =
        slot.use.hazard ? session.TrackHazards().Aims()[*slot.use.hazard].source
                        : session.Racers()[slot.use.racer].Body().motion.position + kBoltSource;
    return {.position = ToEngine(end),
            .rotation = PlacementToEngine(end, source - end, kGameUp).rotation};
}

}

void PowerupParticles::Tick(const sim::RaceSession& session, render::ParticleBank& bank,
                            Milliseconds elapsed) {
    const sim::PowerupSystem& powerups = session.Powerups();
    slots_.resize(powerups.Slots().size());
    for (const sim::PowerupEvent& event : powerups.Events()) {
        if (event.kind == sim::PowerupEventKind::Ended) {
            End(event.effect, powerups.Slots()[event.effect].kind, bank);
        }
    }
    for (const sim::SoundEvent& sound : session.Sounds()) {
        if (sound.sound == sim::RaceSound::LightningStrike) {
            Strike(sound.position, session, bank);
        }
    }
    for (const std::uint16_t slot : powerups.Active()) {
        Follow(slot, session, bank, elapsed);
    }
}

void PowerupParticles::End(std::size_t slot, PowerupKind kind, render::ParticleBank& bank) {
    SlotEmitter& emitter = slots_[slot];
    if (emitter.handle) {
        if (kind == PowerupKind::Cannonball || kind == PowerupKind::Turbo) {
            bank.Release(*emitter.handle);
        } else {
            bank.Finish(*emitter.handle);
        }
    }
    emitter = {};
}

void PowerupParticles::Strike(const Vector3& at, const sim::RaceSession& session,
                              render::ParticleBank& bank) {
    const sim::PowerupSystem& powerups = session.Powerups();
    std::optional<std::size_t> nearest;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const std::uint16_t slot : powerups.Active()) {
        const sim::EffectSlot& effect = powerups.Slots()[slot];
        const float distance = Length(effect.effect->Pose().position - at);
        if (effect.kind == PowerupKind::Lightning && distance < nearestDistance) {
            nearest = slot;
            nearestDistance = distance;
        }
    }
    if (!nearest) {
        return;
    }
    const sim::EffectSlot& bolt = powerups.Slots()[*nearest];
    SlotEmitter& emitter = slots_[*nearest];
    if (emitter.handle) {
        bank.Finish(*emitter.handle);
    }
    emitter.handle = bank.Start(kLightningHit, AtBoltEnd(bolt, session));
}

void PowerupParticles::Follow(std::size_t slot, const sim::RaceSession& session,
                              render::ParticleBank& bank, Milliseconds elapsed) {
    const sim::EffectSlot& effect = session.Powerups().Slots()[slot];
    const std::uint8_t phase = effect.effect->Progress().phase;
    SlotEmitter& emitter = slots_[slot];
    switch (effect.kind) {
        case PowerupKind::Cannonball:
            KeepShotSmoke(emitter, Is(phase, sim::ShotPhase::Flying), AtLauncher(effect, session),
                          bank);
            break;
        case PowerupKind::GrapplingHook:
            KeepShotSmoke(emitter, Is(phase, sim::GrapplePhase::Flying),
                          AtLauncher(effect, session), bank);
            break;
        case PowerupKind::Turbo:
            if (Is(phase, sim::TurboPhase::Boost)) {
                emitter.boost += elapsed;
            }
            Keep(emitter,
                 effect.use.level == kSmokyTurboLevel && Is(phase, sim::TurboPhase::Boost) &&
                     emitter.boost <= kTurboSmokeWindow,
                 kTurboSmoke,
                 AtCar(session.Racers()[effect.use.racer], kTurboSmokeOffset,
                       kTurboSmokeVelocityScale),
                 bank);
            break;
        case PowerupKind::Dynamite:
            Keep(emitter, Is(phase, sim::DynamitePhase::Flying), kDynamiteSparks,
                 AtFuse(effect.effect->Pose()), bank);
            break;
        case PowerupKind::OilSlick:
            Keep(emitter, Is(phase, sim::TrapPhase::Armed), kOilBubbles,
                 {.position = ToEngine(effect.effect->Pose().position + BubbleOffset())}, bank);
            break;
        case PowerupKind::Lightning:
            if (emitter.handle) {
                bank.Place(*emitter.handle, AtBoltEnd(effect, session));
            }
            break;
        default:
            break;
    }
}

void PowerupParticles::Keep(SlotEmitter& emitter, bool shouldStart, std::string_view name,
                            const render::EmitterPlacement& placement, render::ParticleBank& bank) {
    if (!emitter.hasStarted && shouldStart) {
        emitter.hasStarted = true;
        emitter.handle = bank.Start(name, placement);
    } else if (emitter.handle) {
        bank.Place(*emitter.handle, placement);
    }
}

void PowerupParticles::KeepShotSmoke(SlotEmitter& emitter, bool isFlying,
                                     const render::EmitterPlacement& launcher,
                                     render::ParticleBank& bank) {
    Keep(emitter, isFlying, kShotSmoke, launcher, bank);
    if (emitter.handle && bank.SpawnCount(*emitter.handle) >= kShotSmokePuffs) {
        bank.Finish(*emitter.handle);
        emitter.handle.reset();
    }
}

Vector3 PowerupParticles::BubbleOffset() {
    const float angle = random_.Next() * 2.0F * std::numbers::pi_v<float>;
    const float distance = random_.Next() * kBubbleRadius;
    return {.x = std::cos(angle) * distance, .y = std::sin(angle) * distance};
}

}
