#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#include "assets/race_route.hpp"
#include "core/duration.hpp"
#include "sim/ai/ai_craft.hpp"
#include "sim/ai/ai_skill.hpp"
#include "sim/ai/racing_line.hpp"
#include "sim/ai/speed_plan.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/route_playback.hpp"

namespace rr::sim {

/// Unlike `AiRacer`, steers the car physics along the route, drifting where grip can't turn.
class RouteDriver {
public:
    explicit RouteDriver(const assets::RaceRoute& route, std::optional<float> pace = std::nullopt,
                         AiCraft craft = {}, std::optional<SkillProfile> skill = std::nullopt,
                         std::shared_ptr<const RacingLine> line = nullptr);

    /// Paced, keeps to its share of route speed. Lines up `aim`, else passes `racers` but `self`.
    [[nodiscard]] RacerControls Drive(const Racer& racer, std::span<const Vector3> racers = {},
                                      std::optional<std::size_t> self = std::nullopt,
                                      std::optional<Vector3> aim = std::nullopt);

    /// True once `Drive` has made no headway along the route for 3 s. The count then restarts.
    [[nodiscard]] bool IsStalled(Milliseconds elapsed);

    /// On the route a little ahead of where the driver last followed it, facing along it.
    [[nodiscard]] RacerStart Rejoin() const;

    void SetSkill(const SkillProfile& skill) { skill_ = skill; }

    [[nodiscard]] const AiCraft& Craft() const { return craft_; }
    [[nodiscard]] const std::optional<SkillProfile>& Skill() const { return skill_; }
    [[nodiscard]] const RoutePlayback& Nearest() const { return nearest_; }
    [[nodiscard]] const RoutePlayback& Reference() const { return reference_; }
    [[nodiscard]] const RoutePlayback& Ahead() const { return ahead_; }
    [[nodiscard]] Milliseconds ReferenceLead() const { return referenceLead_; }
    [[nodiscard]] Milliseconds AheadLead() const { return aheadLead_; }
    [[nodiscard]] Milliseconds StalledFor() const { return stalled_; }
    [[nodiscard]] bool HasHeadway() const { return hasHeadway_; }
    [[nodiscard]] const std::optional<SpeedPlan>& Plan() const { return plan_; }

private:
    void Follow(const Vector3& position);
    enum class Pedal : std::uint8_t { Throttle, Coast, Brake };

    [[nodiscard]] Pedal PedalFor(const BodyMotion& motion) const;
    [[nodiscard]] float BrakeWindowSpeed() const;
    [[nodiscard]] float LateralOf(const RoutePlayback& playback) const;
    [[nodiscard]] Vector3 LinePosition(const RoutePlayback& playback) const;
    [[nodiscard]] float PassOffset(const BodyMotion& motion, std::span<const Vector3> racers,
                                   std::optional<std::size_t> self) const;
    [[nodiscard]] float AimOffset(const Vector3& aim) const;
    [[nodiscard]] float SideOffset(float wanted);

    RoutePlayback nearest_;
    RoutePlayback reference_;
    RoutePlayback ahead_;
    std::optional<float> pace_;
    AiCraft craft_;
    std::optional<SkillProfile> skill_;
    float wobblePhase_ = 0.0F;
    float sideOffset_ = 0.0F;
    Milliseconds travelled_{0};
    Milliseconds offsetAt_{0};
    std::shared_ptr<const RacingLine> line_;
    std::optional<SpeedPlan> plan_;
    Milliseconds referenceLead_{0};
    Milliseconds aheadLead_{0};
    Milliseconds stalled_{0};
    bool hasHeadway_ = false;
};

}
