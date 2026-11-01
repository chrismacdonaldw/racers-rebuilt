#pragma once

#include <cstdint>

#include "core/vector3.hpp"
#include "sim/powerup/dropped_trap.hpp"
#include "sim/powerup/powerup_effect.hpp"

namespace rr::sim {

class CurseEffect final : public PowerupEffect {
public:
    void Start(const PowerupUse& use, PowerupContext& context) override;
    [[nodiscard]] bool Tick(Milliseconds elapsed, PowerupContext& context) override;
    [[nodiscard]] bool Hit(const PowerupHit& hit, PowerupContext& context) override;
    void End(PowerupContext& context) override;
    [[nodiscard]] EffectProgress Progress() const override;
    [[nodiscard]] Transform Pose() const override { return {.position = modelPosition_}; }

private:
    void Drop(PowerupContext& context);
    void Advance();

    std::uint8_t owner_ = 0;
    std::uint8_t level_ = 0;
    TrapPhase phase_ = TrapPhase::Done;
    Milliseconds remaining_{0};
    Vector3 position_;
    Vector3 modelPosition_;
};

}
