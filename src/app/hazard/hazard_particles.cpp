#include "app/hazard/hazard_particles.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <variant>

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kGhostTrail = "ghsttrl";
constexpr std::string_view kLavaSmoke = "lavasmk";
constexpr std::string_view kVentSmoke = "smoke";
constexpr std::string_view kSnow = "snow";

constexpr Vector3 kGhostTrailDrop{.z = -5.0F};
constexpr Milliseconds kLavaSmokeWait{800};
constexpr std::size_t kVentNode = 0;
constexpr std::array<Vector3, 4> kVentOffsets{{{.x = -24.45F, .y = 26.74F, .z = -19.56F},
                                               {.x = -35.72F, .y = 9.41F, .z = -18.41F},
                                               {.x = -6.9F, .y = -9.13F, .z = -15.49F},
                                               {.x = 4.37F, .y = 8.54F, .z = -16.65F}}};
constexpr Milliseconds kSnowRestart{1000};
constexpr float kSnowAhead = 100.0F;
constexpr float kSnowDrop = 40.0F;
constexpr float kSnowSpread = 200.0F;

render::EmitterPlacement At(const Vector3& game) {
    return {.position = ToEngine(game)};
}

}

void HazardParticles::Tick(const sim::RaceSession& session, const Transform& camera,
                           render::ParticleBank& bank, Milliseconds elapsed) {
    const std::span<const sim::HazardSlot> hazards = session.TrackHazards().Slots();
    slots_.resize(hazards.size());
    for (std::size_t i = 0; i < hazards.size(); ++i) {
        SlotEmitter& emitter = slots_[i];
        emitter.wait = emitter.wait > elapsed ? emitter.wait - elapsed : Milliseconds{0};
        if (hazards[i].phase == sim::HazardPhase::Active) {
            Follow(session, hazards[i].state, camera, emitter, bank);
        } else {
            Stop(emitter, bank);
        }
    }
}

void HazardParticles::Follow(const sim::RaceSession& session, const sim::HazardState& state,
                             const Transform& camera, SlotEmitter& emitter,
                             render::ParticleBank& bank) {
    const sim::TrackEntities& entities = session.Entities();
    if (const auto* ghost = std::get_if<sim::Ghost>(&state); ghost != nullptr && ghost->entity) {
        const Vector3 body = entities.NodeWorld(*ghost->entity, sim::kGhostBodyNode).position;
        Keep(emitter, kGhostTrail, At(body + kGhostTrailDrop), bank);
    } else if (const auto* blob = std::get_if<sim::LavaBlob>(&state);
               blob != nullptr && blob->entity) {
        const float frame = entities.Frame(*blob->entity);
        for (const sim::LavaWindow& window : sim::kLavaWindows) {
            if (emitter.wait == Milliseconds{0} && window.start < frame && frame < window.end) {
                Puff(emitter, kLavaSmoke, At(window.splash), bank);
                emitter.wait = kLavaSmokeWait;
            }
        }
    } else if (const auto* vent = std::get_if<sim::SmokeVent>(&state);
               vent != nullptr && vent->entity) {
        Smoke(entities, *vent->entity, emitter, bank);
    } else if (std::holds_alternative<sim::Snowfall>(state)) {
        if (session.Tints().Of(player_)) {
            Stop(emitter, bank);
        } else {
            Snow(camera, emitter, bank);
        }
    }
}

void HazardParticles::Smoke(const sim::TrackEntities& entities, std::size_t vent,
                            SlotEmitter& emitter, render::ParticleBank& bank) {
    const Transform node = entities.NodeWorld(vent, kVentNode);
    const auto pick = static_cast<std::size_t>(random_.Next() * kVentOffsets.size());
    Keep(emitter, kVentSmoke, At(node.position + Rotate(node.rotation, kVentOffsets.at(pick))),
         bank);
}

void HazardParticles::Snow(const Transform& camera, SlotEmitter& emitter,
                           render::ParticleBank& bank) {
    if (emitter.wait == Milliseconds{0}) {
        Stop(emitter, bank);
        emitter.wait = kSnowRestart;
    }
    const Vector3 forward = Rotate(camera.rotation, {.z = -1.0F});
    const Vector3 right = Rotate(camera.rotation, {.x = 1.0F});
    const Vector3 up = Rotate(camera.rotation, {.y = 1.0F});
    const float side = std::floor(random_.Next() * kSnowSpread) - (kSnowSpread / 2.0F);
    const Vector3 position =
        camera.position + (forward * kSnowAhead) + (right * side) - Vector3{.y = kSnowDrop * up.y};
    Keep(emitter, kSnow, {.position = position}, bank);
}

void HazardParticles::Keep(SlotEmitter& emitter, std::string_view name,
                           const render::EmitterPlacement& placement, render::ParticleBank& bank) {
    if (!emitter.isRunning) {
        emitter.isRunning = true;
        emitter.handle = bank.Start(name, placement);
    } else if (emitter.handle) {
        bank.Place(*emitter.handle, placement);
    }
}

void HazardParticles::Puff(SlotEmitter& emitter, std::string_view name,
                           const render::EmitterPlacement& placement, render::ParticleBank& bank) {
    Stop(emitter, bank);
    emitter.handle = bank.Start(name, placement);
}

void HazardParticles::Stop(SlotEmitter& emitter, render::ParticleBank& bank) {
    if (emitter.handle) {
        bank.Finish(*emitter.handle);
    }
    emitter.handle.reset();
    emitter.isRunning = false;
}

}
