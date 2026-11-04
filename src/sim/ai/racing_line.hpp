#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "assets/race_route.hpp"
#include "core/vector3.hpp"
#include "sim/physics/collision_query.hpp"

namespace rr::sim {

/// A smoother line through a recorded route, as a sideways offset at each route point.
class RacingLine {
public:
    /// Keeps a car's half width clear of the solid walls in `world` and inside the route's width.
    [[nodiscard]] static RacingLine Fit(const assets::RaceRoute& route,
                                        const CollisionQuery& world);

    /// Positive is left of the route. `target` and `alpha` place a car as `RoutePlayback` does.
    [[nodiscard]] float LateralAt(std::size_t target, float alpha) const;

    [[nodiscard]] std::span<const Vector3> Points() const { return points_; }
    [[nodiscard]] std::span<const float> Laterals() const { return laterals_; }

    /// After the last point a closed line goes on from the route's loop point. An open one stops.
    [[nodiscard]] std::size_t Next(std::size_t point) const;

    [[nodiscard]] std::size_t LoopStart() const { return loopStart_; }

    /// The route's last point meets its loop point, so its laps join up.
    [[nodiscard]] bool IsClosed() const { return isClosed_; }

    /// Points where a wall stood inside the route's recorded width.
    [[nodiscard]] std::size_t WalledPoints() const { return walledPoints_; }

private:
    std::vector<Vector3> points_;
    std::vector<float> laterals_;
    std::size_t loopStart_ = 0;
    std::size_t walledPoints_ = 0;
    bool isClosed_ = false;
};

}
