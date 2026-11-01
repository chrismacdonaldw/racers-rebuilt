#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "app/powerup/visual_random.hpp"
#include "core/duration.hpp"
#include "core/vector3.hpp"
#include "render/particle_bank.hpp"
#include "sim/powerup/powerup_system.hpp"
#include "sim/race/race_session.hpp"

namespace rr::app {

/// Tick it after every race tick, or starts and hits are lost.
class PowerupParticles {
public:
    void Tick(const sim::RaceSession& session, render::ParticleBank& bank, Milliseconds elapsed);

private:
    struct SlotEmitter {
        std::optional<render::EmitterHandle> handle;
        bool hasStarted = false;
        Milliseconds boost{0};
    };

    void End(std::size_t slot, sim::PowerupKind kind, render::ParticleBank& bank);
    void Strike(const Vector3& at, const sim::RaceSession& session, render::ParticleBank& bank);
    void Follow(std::size_t slot, const sim::RaceSession& session, render::ParticleBank& bank,
                Milliseconds elapsed);
    static void Keep(SlotEmitter& emitter, bool shouldStart, std::string_view name,
                     const render::EmitterPlacement& placement, render::ParticleBank& bank);
    static void KeepShotSmoke(SlotEmitter& emitter, bool isFlying, const sim::Racer& owner,
                              render::ParticleBank& bank);
    [[nodiscard]] Vector3 BubbleOffset();

    std::vector<SlotEmitter> slots_;
    VisualRandom random_{1U};
};

}
