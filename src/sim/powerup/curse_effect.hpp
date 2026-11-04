#pragma once

#include <cstdint>
#include <optional>

#include "core/quaternion.hpp"
#include "core/vector3.hpp"
#include "sim/powerup/dropped_trap.hpp"
#include "sim/powerup/powerup_effect.hpp"

namespace rr::sim {

inline constexpr float kCurseModelHeight = 13.0F;

class CurseEffect final : public PowerupEffect {
public:
    void Start(const PowerupUse& use, PowerupContext& context) override;
    [[nodiscard]] bool Tick(Milliseconds elapsed, PowerupContext& context) override;
    [[nodiscard]] bool Hit(const PowerupHit& hit, PowerupContext& context) override;
    void End(PowerupContext& context) override;
    [[nodiscard]] EffectProgress Progress() const override;
    [[nodiscard]] Transform Pose() const override {
        return {.position = modelPosition_, .rotation = rotation_};
    }

private:
    void Drop(PowerupContext& context);
    void Advance();

    const CurseTuning* tuning_ = &kOriginalPowerups.curse;
    std::optional<std::uint8_t> owner_;
    std::uint8_t level_ = 0;
    TrapPhase phase_ = TrapPhase::Done;
    Milliseconds remaining_{0};
    Vector3 position_;
    Vector3 modelPosition_;
    Quaternion rotation_;
};

}
