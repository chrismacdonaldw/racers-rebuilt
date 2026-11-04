#pragma once

#include <cstdint>

#include "sim/powerup/ballistic_flight.hpp"
#include "sim/powerup/powerup_effect.hpp"

namespace rr::sim {

enum class GrapplePhase : std::uint8_t { Flying, Pulling, Retracting, Done };

class GrappleEffect final : public PowerupEffect {
public:
    void Start(const PowerupUse& use, PowerupContext& context) override;
    [[nodiscard]] bool Tick(Milliseconds elapsed, PowerupContext& context) override;
    [[nodiscard]] bool Hit(const PowerupHit& hit, PowerupContext& context) override;
    void End(PowerupContext& context) override;
    [[nodiscard]] EffectProgress Progress() const override;
    [[nodiscard]] Transform Pose() const override;

private:
    void Fly(Milliseconds elapsed, PowerupContext& context);
    void Hook(std::uint8_t victim, PowerupContext& context);
    void Pull(Milliseconds elapsed, PowerupContext& context);
    void HoldOrRelease(PowerupContext& context);
    void Release(const Vector3& at, PowerupContext& context);
    void Retract(Milliseconds elapsed, const PowerupContext& context);
    void EndPull(PowerupContext& context);
    void TimeOut(PowerupContext& context);
    [[nodiscard]] Vector3 HookPosition() const;

    const GrappleTuning* tuning_ = &kOriginalPowerups.grapple;
    std::uint8_t owner_ = 0;
    std::uint8_t victim_ = 0;
    bool hasTouched_ = false;
    GrapplePhase phase_ = GrapplePhase::Done;
    Milliseconds timer_{0};
    BallisticFlight flight_;
    Vector3 hook_;
    float tension_ = 0.0F;
    bool isSnapped_ = false;
    bool isPulling_ = false;
};

}
