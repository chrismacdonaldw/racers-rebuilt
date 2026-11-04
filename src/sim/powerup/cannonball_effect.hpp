#pragma once

#include <cstdint>
#include <optional>

#include "sim/powerup/ballistic_flight.hpp"
#include "sim/powerup/powerup_effect.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {

class CannonballEffect final : public PowerupEffect {
public:
    void Start(const PowerupUse& use, PowerupContext& context) override;
    [[nodiscard]] bool Tick(Milliseconds elapsed, PowerupContext& context) override;
    [[nodiscard]] bool Hit(const PowerupHit& hit, PowerupContext& context) override;
    void End(PowerupContext& context) override;
    [[nodiscard]] EffectProgress Progress() const override;
    [[nodiscard]] Transform Pose() const override { return {.position = flight_.Position()}; }

private:
    [[nodiscard]] Vector3 LaunchFrom(const Racer& owner, const PowerupContext& context);
    [[nodiscard]] Vector3 LaunchFrom(const HazardAim& aim);

    const CannonballTuning* tuning_ = &kOriginalPowerups.cannonball;
    std::optional<std::uint8_t> owner_;
    ShotPhase phase_ = ShotPhase::Done;
    BallisticFlight flight_;
};

}
