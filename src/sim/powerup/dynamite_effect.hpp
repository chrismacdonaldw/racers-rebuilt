#pragma once

#include <cstdint>
#include <optional>

#include "core/vector3.hpp"
#include "sim/powerup/ballistic_flight.hpp"
#include "sim/powerup/powerup_effect.hpp"

namespace rr::sim {

enum class DynamitePhase : std::uint8_t {
    Lit = 2,
    Flying = 3,
    Blasting = 4,
    LastBlast = 5,
    Done = 6
};

class DynamiteEffect final : public PowerupEffect {
public:
    void Start(const PowerupUse& use, PowerupContext& context) override;
    [[nodiscard]] bool Tick(Milliseconds elapsed, PowerupContext& context) override;
    [[nodiscard]] bool Hit(const PowerupHit& hit, PowerupContext& context) override;
    void End(PowerupContext& context) override;
    [[nodiscard]] EffectProgress Progress() const override;
    [[nodiscard]] Transform Pose() const override;

private:
    void Launch(const PowerupContext& context);
    [[nodiscard]] bool HasLanded(Milliseconds elapsed, const PowerupContext& context);
    void Advance(PowerupContext& context);
    void Blast(PowerupContext& context, const Vector3& at) const;
    [[nodiscard]] Vector3 Scattered(PowerupContext& context) const;

    const DynamiteTuning* tuning_ = &kOriginalPowerups.dynamite;
    std::uint8_t owner_ = 0;
    std::optional<std::uint8_t> target_;
    DynamitePhase phase_ = DynamitePhase::Done;
    Milliseconds remaining_{0};
    BallisticFlight flight_;
    Vector3 landing_;
    float tumble_ = 0.0F;
};

}
