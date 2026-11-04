#pragma once

#include <cstdint>
#include <optional>

#include "core/quaternion.hpp"
#include "core/vector3.hpp"
#include "sim/powerup/powerup_effect.hpp"

namespace rr::sim {

enum class MagnetPhase : std::uint8_t { Armed = 2, Holding = 3, Fading = 4, Done = 6 };

/// Holds one racer at a time, and pulls one other in reach toward the drop point each tick.
class MagnetEffect final : public PowerupEffect {
public:
    void Start(const PowerupUse& use, PowerupContext& context) override;
    [[nodiscard]] bool Tick(Milliseconds elapsed, PowerupContext& context) override;
    [[nodiscard]] bool Hit(const PowerupHit& hit, PowerupContext& context) override;
    void End(PowerupContext& context) override;
    [[nodiscard]] EffectProgress Progress() const override;
    [[nodiscard]] Transform Pose() const override;

private:
    void Drop(PowerupContext& context);
    void Touch(Racer& racer, std::uint8_t index, PowerupContext& context);
    void Expire(PowerupContext& context);
    void Hold(Racer& racer, Milliseconds elapsed);
    void Pull(Racer& racer, Milliseconds elapsed) const;
    void Face(const PowerupContext& context);
    [[nodiscard]] float PullFor(Milliseconds elapsed) const;

    const MagnetTuning* tuning_ = &kOriginalPowerups.magnet;
    std::uint8_t owner_ = 0;
    MagnetPhase phase_ = MagnetPhase::Done;
    Milliseconds remaining_{0};
    bool isDropped_ = false;
    Vector3 dropPoint_;
    Vector3 heading_{.x = 1.0F};
    Quaternion facing_;
    std::optional<std::uint8_t> held_;
    std::optional<std::uint8_t> pulled_;
    bool hasStopped_ = false;
    bool hasLifted_ = false;
};

}
