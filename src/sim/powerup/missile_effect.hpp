#pragma once

#include <cstdint>

#include "sim/powerup/homing_flight.hpp"
#include "sim/powerup/powerup_effect.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {

/// Needs the context's checkpoints and progress. Shot `i` homes on the (i + 1)th racer ahead.
class MissileEffect final : public PowerupEffect {
public:
    void Start(const PowerupUse& use, PowerupContext& context) override;
    [[nodiscard]] bool Tick(Milliseconds elapsed, PowerupContext& context) override;
    [[nodiscard]] bool Hit(const PowerupHit& hit, PowerupContext& context) override;
    void End(PowerupContext& context) override;
    [[nodiscard]] EffectProgress Progress() const override;
    [[nodiscard]] Transform Pose() const override;

private:
    void Launch(const PowerupContext& context);
    [[nodiscard]] bool Fly(Milliseconds elapsed, PowerupContext& context);

    const MissileTuning* tuning_ = &kOriginalPowerups.missile;
    std::uint8_t owner_ = 0;
    std::uint8_t shot_ = 0;
    ShotPhase phase_ = ShotPhase::Done;
    Milliseconds armed_{0};
    Transform carried_;
    HomingFlight flight_;
};

}
