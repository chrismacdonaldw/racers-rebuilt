#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "sim/powerup/powerup_effect.hpp"

namespace rr::sim {

enum class WarpPhase : std::uint8_t { Starting, Active, Done };

/// Needs the context's checkpoints and progress, unless a hazard aims it.
class WarpEffect final : public PowerupEffect {
public:
    void Start(const PowerupUse& use, PowerupContext& context) override;
    [[nodiscard]] bool Tick(Milliseconds elapsed, PowerupContext& context) override;
    [[nodiscard]] bool Hit(const PowerupHit& hit, PowerupContext& context) override;
    void End(PowerupContext& context) override;
    [[nodiscard]] EffectProgress Progress() const override;
    [[nodiscard]] Transform Pose() const override { return pose_; }

private:
    void Advance(PowerupContext& context);
    void Land(PowerupContext& context);
    [[nodiscard]] std::optional<std::size_t> NextGate(const PowerupContext& context) const;

    void Lerp(PowerupContext& context, Milliseconds elapsed, const Vector3& to);

    std::uint8_t racer_ = 0;
    std::optional<HazardAim> aim_;
    Vector3 from_;
    WarpPhase phase_ = WarpPhase::Done;
    Milliseconds remaining_{0};
    Transform pose_;
};

}
