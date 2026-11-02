#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "app/powerup/visual_random.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"
#include "render/particle_bank.hpp"
#include "sim/hazard/hazards.hpp"
#include "sim/race/race_session.hpp"

namespace rr::app {

/// Tick it after every race tick, with the player's camera in engine space.
class HazardParticles {
public:
    explicit HazardParticles(std::size_t player) : player_(player) {}

    void Tick(const sim::RaceSession& session, const Transform& camera, render::ParticleBank& bank,
              Milliseconds elapsed);

private:
    struct SlotEmitter {
        std::optional<render::EmitterHandle> handle;
        bool isRunning = false;
        Milliseconds wait{0};
    };

    void Follow(const sim::RaceSession& session, const sim::HazardState& state,
                const Transform& camera, SlotEmitter& emitter, render::ParticleBank& bank);
    void Smoke(const sim::TrackEntities& entities, std::size_t vent, bool isMirrored,
               SlotEmitter& emitter, render::ParticleBank& bank);
    void Snow(const Transform& camera, SlotEmitter& emitter, render::ParticleBank& bank);
    static void Keep(SlotEmitter& emitter, std::string_view name,
                     const render::EmitterPlacement& placement, render::ParticleBank& bank);
    static void Puff(SlotEmitter& emitter, std::string_view name,
                     const render::EmitterPlacement& placement, render::ParticleBank& bank);
    static void Stop(SlotEmitter& emitter, render::ParticleBank& bank);

    std::size_t player_;
    std::vector<SlotEmitter> slots_;
    VisualRandom random_{1U};
};

}
