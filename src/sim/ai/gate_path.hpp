#pragma once

#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "assets/checkpoints.hpp"
#include "assets/race_route.hpp"
#include "core/vector3.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/racer.hpp"

namespace rr::sim {

struct GatePoint {
    Vector3 position;
    float widthLeft = 0.0F;
    float widthRight = 0.0F;
    bool isAirborne = false;
};

/// A path once around a track's checkpoint loop by first links, kept between its walls and drops.
class GateLoop {
public:
    /// Empty unless first links lead from checkpoint 0 back to it.
    [[nodiscard]] static std::optional<GateLoop> Trace(std::span<const assets::Checkpoint> gates,
                                                       const CollisionQuery& world);

    /// The last point leads back to the first.
    [[nodiscard]] std::span<const GatePoint> Points() const { return points_; }

    /// Drives from `start` onto the loop just ahead of it, then laps the loop.
    [[nodiscard]] assets::RaceRoute RouteFrom(const RacerStart& start,
                                              const CollisionQuery& world) const;

private:
    explicit GateLoop(std::vector<GatePoint> points) : points_(std::move(points)) {}

    std::vector<GatePoint> points_;
};

}
