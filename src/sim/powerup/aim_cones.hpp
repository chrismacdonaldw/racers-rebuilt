#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include "assets/target_points.hpp"
#include "core/vector3.hpp"
#include "sim/physics/racer.hpp"

namespace rr::sim {

struct AimCone {
    float minDistanceSquared = 0.0F;
    float maxDistanceSquared = 0.0F;
    float cosine = 0.0F;
};

inline constexpr AimCone kRacerAheadCone{
    .minDistanceSquared = 100.0F, .maxDistanceSquared = 160'000.0F, .cosine = 0.9F};
inline constexpr AimCone kTargetPointCone{
    .minDistanceSquared = 100.0F, .maxDistanceSquared = 160'000.0F, .cosine = 0.95F};
inline constexpr AimCone kMissileCone{
    .minDistanceSquared = 100.0F, .maxDistanceSquared = 160'000.0F, .cosine = 0.7071F};

/// Searches in racer order from `first` like the original, so the match isn't the nearest racer.
[[nodiscard]] std::optional<std::size_t> FindRacerInCone(std::span<const Racer> racers,
                                                         const Vector3& origin,
                                                         const Vector3& direction,
                                                         const AimCone& cone,
                                                         std::size_t first = 0);

[[nodiscard]] std::optional<std::size_t> FindNearestRacerInCone(std::span<const Racer> racers,
                                                                const Vector3& origin,
                                                                const Vector3& direction,
                                                                const AimCone& cone);

[[nodiscard]] std::optional<std::size_t> FindFarthestRacerInCone(std::span<const Racer> racers,
                                                                 const Vector3& origin,
                                                                 const Vector3& direction,
                                                                 const AimCone& cone);

[[nodiscard]] std::optional<std::size_t> FindNearestPositionInCone(
    std::span<const Vector3> positions, std::size_t self, const Vector3& origin,
    const Vector3& direction, const AimCone& cone);

[[nodiscard]] std::optional<Vector3> FindNearestTargetPoint(
    std::span<const assets::TargetPoint> points, const Vector3& origin, const Vector3& direction,
    const AimCone& cone);

}
