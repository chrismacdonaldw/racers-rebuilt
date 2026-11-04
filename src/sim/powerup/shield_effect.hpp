#pragma once

#include <cstdint>

#include "sim/powerup/powerup_effect.hpp"

namespace rr::sim {

/// The racer holds the shield, so a newer shield on the same racer ends this one.
class ShieldEffect final : public PowerupEffect {
public:
    void Start(const PowerupUse& use, PowerupContext& context) override;
    [[nodiscard]] bool Tick(Milliseconds elapsed, PowerupContext& context) override;
    [[nodiscard]] bool Hit(const PowerupHit& hit, PowerupContext& context) override;
    void End(PowerupContext& context) override;
    [[nodiscard]] EffectProgress Progress() const override { return progress_; }
    [[nodiscard]] Transform Pose() const override { return pose_; }

private:
    [[nodiscard]] bool IsHeld(const Racer& racer) const;
    void Follow(const Racer& racer);

    std::uint8_t racer_ = 0;
    std::uint32_t raised_ = 0;
    Milliseconds duration_{0};
    EffectProgress progress_;
    Transform pose_;
};

/// Only cannonballs and missiles turn back.
[[nodiscard]] bool TurnsShotsBack(const Racer& racer, const ShieldTuning& tuning);

void AddBlueEffects(EffectFactories& factories);

}
