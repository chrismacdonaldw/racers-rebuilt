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

/// Dust, exhaust and tyre smoke follow the player's car; every car lands and sparks.
class CarParticles {
public:
    void Tick(std::span<const sim::Racer> racers, render::ParticleBank& bank);

    /// `velocity` is the one the player's wheel emitters get.
    void Update(const sim::Racer& player, const std::optional<SkidWheels>& skidding,
                bool isRearKicking, const Vector3& velocity, render::ParticleBank& bank);

private:
    struct Landing {
        bool isFalling = false;
        int contacts = 0;
    };

    void StartPuffs(const sim::Racer& player, bool isRearKicking,
                    const render::EmitterPlacement& moving, render::ParticleBank& bank);
    void UpdateTireSmoke(const sim::Racer& player, const std::optional<SkidWheels>& skidding,
                         const render::EmitterPlacement& moving, render::ParticleBank& bank);
    static void Puff(std::optional<render::EmitterHandle>& handle, std::size_t puffs,
                     const render::EmitterPlacement& placement, render::ParticleBank& bank);

    std::vector<Landing> landings_;
    std::uint32_t turboCount_ = 0;
    bool wasDrifting_ = false;
    std::optional<render::EmitterHandle> dust_;
    std::optional<render::EmitterHandle> exhaust_;
    std::optional<render::EmitterHandle> tireSmoke_;
    VisualRandom random_{1U};
};

}
