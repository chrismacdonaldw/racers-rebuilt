#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "core/bounding_sphere.hpp"
#include "sim/powerup/powerup_effect.hpp"

namespace rr::sim {

enum class LightningPhase : std::uint8_t { RampIn, Sustain, Fade, Done };

class LightningEffect final : public PowerupEffect {
public:
    void Start(const PowerupUse& use, PowerupContext& context) override;
    [[nodiscard]] bool Tick(Milliseconds elapsed, PowerupContext& context) override;
    [[nodiscard]] bool Hit(const PowerupHit& hit, PowerupContext& context) override;
    void End(PowerupContext& context) override;
    [[nodiscard]] EffectProgress Progress() const override;
    [[nodiscard]] Transform Pose() const override { return {.position = bolt_.back()}; }

private:
    void Advance(PowerupContext& context);
    [[nodiscard]] float SegmentLength() const;
    void TraceBolt(const PowerupContext& context);
    void FindVictim(PowerupContext& context);
    void Strike(std::uint8_t racer, PowerupContext& context);
    [[nodiscard]] bool IsOnBolt(const BoundingSphere& reach) const;

    std::optional<std::uint8_t> owner_;
    std::optional<std::uint8_t> hazard_;
    LightningPhase phase_ = LightningPhase::Done;
    Milliseconds timer_{0};
    std::uint8_t victim_ = 0;
    bool isHolding_ = false;
    Milliseconds held_{0};
    Vector3 origin_;
    Vector3 forward_;
    std::array<Vector3, 4> bolt_{};
};

}
