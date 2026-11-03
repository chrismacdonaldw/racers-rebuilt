#include "sim/powerup/aim_cones.hpp"

namespace rr::sim {
namespace {

struct Sight {
    Vector3 origin;
    Vector3 direction;
    AimCone cone;
};

std::optional<float> DistanceSquaredInCone(const Sight& sight, const Vector3& at) {
    const Vector3 delta = at - sight.origin;
    const float distanceSquared = Dot(delta, delta);
    if (distanceSquared < sight.cone.minDistanceSquared ||
        distanceSquared > sight.cone.maxDistanceSquared ||
        Dot(sight.direction, Normalized(delta, {})) < sight.cone.cosine) {
        return std::nullopt;
    }
    return distanceSquared;
}

template <typename Item, typename PositionOf, typename IsBetter>
std::optional<std::size_t> FindBest(std::span<const Item> items, const Sight& sight,
                                    PositionOf positionOf, IsBetter isBetter,
                                    std::optional<std::size_t> skip = std::nullopt) {
    std::optional<std::size_t> best;
    float bestDistanceSquared = 0.0F;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i == skip) {
            continue;
        }
        const std::optional<float> distanceSquared =
            DistanceSquaredInCone(sight, positionOf(items[i]));
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
        if (DistanceSquaredInCone({.origin = origin, .direction = direction, .cone = cone},
                                  RacerPosition(racers[i]))) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> FindNearestRacerInCone(std::span<const Racer> racers,
                                                  const Vector3& origin, const Vector3& direction,
                                                  const AimCone& cone) {
    return FindBest(racers, {.origin = origin, .direction = direction, .cone = cone}, RacerPosition,
                    IsNearer);
}

std::optional<std::size_t> FindFarthestRacerInCone(std::span<const Racer> racers,
                                                   const Vector3& origin, const Vector3& direction,
                                                   const AimCone& cone) {
    return FindBest(racers, {.origin = origin, .direction = direction, .cone = cone}, RacerPosition,
                    IsFarther);
}

std::optional<std::size_t> FindNearestPositionInCone(std::span<const Vector3> positions,
                                                     std::size_t self, const Vector3& origin,
                                                     const Vector3& direction,
                                                     const AimCone& cone) {
    return FindBest(
        positions, {.origin = origin, .direction = direction, .cone = cone},
        [](const Vector3& at) { return at; }, IsNearer, self);
}

std::optional<Vector3> FindNearestTargetPoint(std::span<const assets::TargetPoint> points,
                                              const Vector3& origin, const Vector3& direction,
                                              const AimCone& cone) {
    const std::optional<std::size_t> nearest = FindBest(
        points, {.origin = origin, .direction = direction, .cone = cone},
        [](const assets::TargetPoint& point) -> const Vector3& { return point.position; },
        IsNearer);
    return nearest ? std::optional(points[*nearest].position) : std::nullopt;
}

}
