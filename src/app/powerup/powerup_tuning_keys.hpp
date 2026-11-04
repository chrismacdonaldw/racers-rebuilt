#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <string_view>

#include "core/duration.hpp"
#include "sim/powerup/powerup_tuning.hpp"

namespace rr::app::powerup_keys {

struct Limits {
    double min = 0.0;
    double max = 0.0;
};

inline constexpr Limits kTimes{.min = 1.0, .max = 600'000.0};
inline constexpr Limits kSpeeds{.min = 1.0, .max = 2000.0};
inline constexpr Limits kGravities{.min = -1000.0, .max = 0.0};
inline constexpr Limits kDistances{.min = 0.0, .max = 2000.0};
inline constexpr Limits kOffsets{.min = -2000.0, .max = 2000.0};
inline constexpr Limits kCosines{.min = -1.0, .max = 1.0};
inline constexpr Limits kScales{.min = 0.0, .max = 10.0};
inline constexpr Limits kSpinRates{.min = 0.0001, .max = 1.0};
inline constexpr Limits kForces{.min = 0.0, .max = 10'000.0};
inline constexpr Limits kPerMs{.min = 0.0, .max = 100.0};
inline constexpr Limits kSegments{.min = 1.0, .max = 500.0};
inline constexpr Limits kScatter{.min = 0.0, .max = 100.0};
inline constexpr Limits kLevels{.min = 0.0, .max = sim::kMaxPowerupLevel + 1.0};
inline constexpr Limits kWalks{.min = 0.01, .max = 10.0};
inline constexpr Limits kRouteSpeeds{.min = 0.1, .max = 50.0};

inline constexpr std::string_view kSlotsSection = "slots";

inline constexpr std::array<std::string_view, sim::kPowerupKindCount> kKindNames{
    "cannonball", "grappling_hook", "lightning", "missile", "oil_slick", "dynamite",
    "magnet",     "curse",          "shield",    "turbo",   "warp"};

inline constexpr std::array<std::string_view, sim::kPowerupColourCount> kColourNames{
    "red", "blue", "green", "yellow"};

inline constexpr std::array<std::string_view, 3> kTurboNames{"weak", "strong", "hover"};

template <std::size_t N>
[[nodiscard]] std::optional<std::size_t> IndexOf(const std::array<std::string_view, N>& names,
                                                 std::string_view name) {
    const auto found = std::ranges::find(names, name);
    return found == names.end() ? std::nullopt
                                : std::optional(static_cast<std::size_t>(found - names.begin()));
}

void VisitShot(auto& visitor, std::string_view prefix, auto& shot) {
    visitor.Number(std::format("{}speed", prefix), shot.speed, kSpeeds);
    visitor.Number(std::format("{}gravity", prefix), shot.gravity, kGravities);
    visitor.Time(std::format("{}lifetime_ms", prefix), shot.lifetime, kTimes);
}

void VisitCone(auto& visitor, std::string_view name, auto& cone) {
    visitor.Distance(std::format("{}_min", name), cone.minDistanceSquared, kDistances);
    visitor.Distance(std::format("{}_max", name), cone.maxDistanceSquared, kDistances);
    visitor.Number(std::format("{}_cosine", name), cone.cosine, kCosines);
}

void VisitRed(auto& visitor, auto& tuning) {
    auto& cannonball = tuning.cannonball;
    visitor.Section("cannonball");
    VisitShot(visitor, "", cannonball.shot);
    VisitShot(visitor, "hazard_", cannonball.hazardShot);
    VisitCone(visitor, "racer_cone", cannonball.racerCone);
    VisitCone(visitor, "target_cone", cannonball.pointCone);
    visitor.Number("range", cannonball.range, kDistances);

    auto& grapple = tuning.grapple;
    visitor.Section("grappling_hook");
    VisitShot(visitor, "", grapple.hook);
    VisitCone(visitor, "far_cone", grapple.farCone);
    VisitCone(visitor, "near_cone", grapple.nearCone);
    VisitCone(visitor, "target_cone", grapple.pointCone);
    visitor.Number("range", grapple.range, kDistances);
    visitor.Time("pull_ms", grapple.pullTime, kTimes);
    visitor.Time("retract_ms", grapple.retractTime, kTimes);
    visitor.Number("pull_force", grapple.pullForce, kForces);
    visitor.Distance("release_distance", grapple.releaseDistanceSquared, kDistances);
    visitor.Number("chase_speed", grapple.chaseSpeed, kSpeeds);
    visitor.Number("retract_speed", grapple.retractSpeed, kSpeeds);
    visitor.Number("tension_per_second", grapple.tensionPerSecond, kPerMs);

    auto& lightning = tuning.lightning;
    visitor.Section("lightning");
    visitor.Time("ramp_ms", lightning.rampTime, kTimes);
    visitor.Time("sustain_ms", lightning.sustainTime, kTimes);
    visitor.Time("fade_ms", lightning.fadeTime, kTimes);
    visitor.Time("hold_ms", lightning.holdTime, kTimes);
    visitor.Number("segment_length", lightning.segmentLength, kSegments);
    VisitCone(visitor, "victim_cone", lightning.victimCone);
    visitor.Number("hit_strength", lightning.hitStrength, kScales);

    auto& missile = tuning.missile;
    visitor.Section("missile");
    VisitShot(visitor, "", missile.shot);
    visitor.Times("arm_ms", missile.armTimes, kTimes);
    visitor.Numbers("side_offsets", missile.sideOffsets, kOffsets);
    visitor.Number("range", missile.range, kDistances);
    visitor.Number("spin_turns", missile.spinTurns, kScales);
    visitor.Number("spin_rate_per_ms", missile.spinRatePerMs, kSpinRates);
}

void VisitYellow(auto& visitor, auto& tuning) {
    auto& oil = tuning.oilSlick;
    visitor.Section("oil_slick");
    visitor.Number("radius", oil.radius, kDistances);
    visitor.Time("lifetime_ms", oil.lifetime, kTimes);
    visitor.Number("spin_turns", oil.spinTurns, kScales);
    visitor.Number("spin_rate_per_ms", oil.spinRatePerMs, kSpinRates);
    visitor.Number("spin_align", oil.spinAlign, kScales);

    auto& dynamite = tuning.dynamite;
    visitor.Section("dynamite");
    VisitShot(visitor, "", dynamite.shot);
    VisitCone(visitor, "target_cone", dynamite.targetCone);
    visitor.Number("range", dynamite.range, kDistances);
    visitor.Time("fuse_ms", dynamite.fuse, kTimes);
    visitor.Time("blast_interval_ms", dynamite.blastInterval, kTimes);
    visitor.Count("scatter", dynamite.scatter, kScatter);

    auto& magnet = tuning.magnet;
    visitor.Section("magnet");
    visitor.Number("radius", magnet.radius, kDistances);
    visitor.Time("armed_ms", magnet.armed, kTimes);
    visitor.Time("holding_ms", magnet.holding, kTimes);
    visitor.Time("fade_ms", magnet.fade, kTimes);
    visitor.Time("halted_hold_ms", magnet.haltedHold, kTimes);
    visitor.Distance("grab_distance", magnet.grabDistanceSquared, kDistances);
    visitor.Distance("lift_stop_distance", magnet.liftStopDistanceSquared, kDistances);
    visitor.Number("stopped_speed", magnet.stoppedSpeed, kPerMs);
    visitor.Number("pull_per_ms", magnet.pullPerMs, kPerMs);

    auto& curse = tuning.curse;
    visitor.Section("curse");
    visitor.Number("radius", curse.radius, kDistances);
    visitor.Time("active_ms", curse.active, kTimes);
    visitor.Time("fade_ms", curse.fade, kTimes);
    visitor.Time("curse_ms", curse.curse, kTimes);
}

void VisitBlueAndGreen(auto& visitor, auto& tuning) {
    auto& shield = tuning.shield;
    visitor.Section("shield");
    visitor.Times("durations_ms", shield.durations, kTimes);
    visitor.Time("fade_ms", shield.fade, kTimes);
    visitor.Count("deflect_level", shield.deflectLevel, kLevels);

    visitor.Section("turbo");
    visitor.Turbos("levels", tuning.turbo.levels);

    auto& warp = tuning.warp;
    visitor.Section("warp");
    visitor.Time("starting_ms", warp.startingTime, kTimes);
    visitor.Time("active_ms", warp.activeTime, kTimes);
    visitor.Number("walk_per_ms", warp.pace.walkPerMs, kWalks);
    visitor.Number("route_speed", warp.pace.routeSpeed, kRouteSpeeds);
}

/// Calls `visitor` for every `POWERUPS.TOML` key but the slots, ending with an empty section.
void VisitTuning(auto& visitor, auto& tuning) {
    VisitRed(visitor, tuning);
    VisitYellow(visitor, tuning);
    VisitBlueAndGreen(visitor, tuning);
    visitor.Section({});
}

}
