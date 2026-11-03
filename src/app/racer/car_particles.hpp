#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "app/powerup/visual_random.hpp"
#include "app/racer/skid_marks.hpp"
#include "core/vector3.hpp"
#include "render/particle_bank.hpp"
#include "sim/physics/racer.hpp"

namespace rr::app {

/// Every car puffs dust and exhaust on a turbo, lands and sparks; players also on drifts and skids.
class CarParticles {
public:
    struct Puffs {
        std::uint32_t turboCount = 0;
        bool wasDrifting = false;
        std::optional<render::EmitterHandle> dust;
        std::optional<render::EmitterHandle> exhaust;
        std::optional<render::EmitterHandle> tireSmoke;
    };

    struct PuffSource {
        std::optional<SkidWheels> skidding;
        bool isRearKicking = false;
        bool isPlayer = false;
        Vector3 velocity;
    };

    void Tick(std::span<const sim::Racer> racers, render::ParticleBank& bank);
    void Update(const sim::Racer& racer, Puffs& puffs, const PuffSource& source,
                render::ParticleBank& bank);

private:
    struct Landing {
        bool isFalling = false;
        int contacts = 0;
    };

    static void StartPuffs(const sim::Racer& racer, Puffs& puffs, const PuffSource& source,
                           const render::EmitterPlacement& moving, render::ParticleBank& bank);
    void UpdateTireSmoke(const sim::Racer& player, Puffs& puffs,
                         const std::optional<SkidWheels>& skidding,
                         const render::EmitterPlacement& moving, render::ParticleBank& bank);
    static void Puff(std::optional<render::EmitterHandle>& handle, std::size_t puffs,
                     const render::EmitterPlacement& placement, render::ParticleBank& bank);

    std::vector<Landing> landings_;
    VisualRandom random_{1U};
};

}
