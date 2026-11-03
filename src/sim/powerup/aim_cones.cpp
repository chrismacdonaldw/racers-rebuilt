#include "sim/powerup/aim_cones.hpp"

namespace rr::sim {
namespace {

std::optional<float> DistanceSquaredInCone(const Vector3& origin, const Vector3& direction,
                                           const AimCone& cone, const Vector3& at) {
    const Vector3 delta = at - origin;
    const float distanceSquared = Dot(delta, delta);
    if (distanceSquared < cone.minDistanceSquared || distanceSquared > cone.maxDistanceSquared ||
        Dot(direction, Normalized(delta, {})) < cone.cosine) {
        return std::nullopt;
    }
    return distanceSquared;
}

template <typename Item, typename PositionOf, typename IsBetter>
std::optional<std::size_t> FindBest(std::span<const Item> items, const Vector3& origin,
                                    const Vector3& direction, const AimCone& cone,
                                    PositionOf positionOf, IsBetter isBetter) {
    std::optional<std::size_t> best;
    float bestDistanceSquared = 0.0F;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const std::optional<float> distanceSquared =
            DistanceSquaredInCone(origin, direction, cone, positionOf(items[i]));
        if (distanceSquared && (!best || isBetter(*distanceSquared, bestDistanceSquared))) {
            best = i;
            bestDistanceSquared = *distanceSquared;
        }
    }
    return best;
}

const Vector3& RacerPosition(const Racer& racer) {
    return racer.Body().motion.position;
}

bool IsNearer(float a, float b) {
    return a < b;
}

bool IsFarther(float a, float b) {
    return a > b;
}

}

std::optional<std::size_t> FindRacerInCone(std::span<const Racer> racers, const Vector3& origin,
                                           const Vector3& direction, const AimCone& cone,
                                           std::size_t first) {
    for (std::size_t i = first; i < racers.size(); ++i) {
        if (DistanceSquaredInCone(origin, direction, cone, RacerPosition(racers[i]))) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> FindNearestRacerInCone(std::span<const Racer> racers,
                                                  const Vector3& origin, const Vector3& direction,
                                                  const AimCone& cone) {
    return FindBest(racers, origin, direction, cone, RacerPosition, IsNearer);
}

std::optional<std::size_t> FindFarthestRacerInCone(std::span<const Racer> racers,
                                                   const Vector3& origin, const Vector3& direction,
                                                   const AimCone& cone) {
    return FindBest(racers, origin, direction, cone, RacerPosition, IsFarther);
}

std::optional<std::size_t> FindNearestPositionInCone(std::span<const Vector3> positions,
                                                     const Vector3& origin,
                                                     const Vector3& direction,
                                                     const AimCone& cone) {
    return FindBest(
        positions, origin, direction, cone, [](const Vector3& at) { return at; }, IsNearer);
}

std::optional<Vector3> FindNearestTargetPoint(std::span<const assets::TargetPoint> points,
                                              const Vector3& origin, const Vector3& direction,
                                              const AimCone& cone) {
    const std::optional<std::size_t> nearest = FindBest(
        points, origin, direction, cone,
        [](const assets::TargetPoint& point) -> const Vector3& { return point.position; },
        IsNearer);
    return nearest ? std::optional(points[*nearest].position) : std::nullopt;
}

}
