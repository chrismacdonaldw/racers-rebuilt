#pragma once

#include <cstdint>

#include "core/vector3.hpp"
#include "sim/powerup/dropped_trap.hpp"
#include "sim/powerup/powerup_effect.hpp"

namespace rr::sim {

class OilSlickEffect final : public PowerupEffect {
public:
    void Start(const PowerupUse& use, PowerupContext& context) override;
    [[nodiscard]] bool Tick(Milliseconds elapsed, PowerupContext& context) override;
    [[nodiscard]] bool Hit(const PowerupHit& hit, PowerupContext& context) override;
    void End(PowerupContext& context) override;
    [[nodiscard]] EffectProgress Progress() const override;
    [[nodiscard]] Transform Pose() const override { return {.position = position_}; }

private:
    void Drop(PowerupContext& context);

    std::uint8_t owner_ = 0;
    TrapPhase phase_ = TrapPhase::Done;
    Milliseconds remaining_{0};
    Vector3 position_;
};

}
