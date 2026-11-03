#pragma once

#include <span>
#include <vector>

#include "assets/target_points.hpp"
#include "sim/track/track_event.hpp"

namespace rr::sim {

/// The track's target points that aimed shots may still pick.
class AimTargets {
public:
    explicit AimTargets(std::span<const assets::TargetPoint> points)
        : left_(points.begin(), points.end()) {}

    /// Only a position-path start fired on its own removes points, as in the original.
    void Hear(std::span<const TrackEvent> events);

    [[nodiscard]] std::span<const assets::TargetPoint> Left() const { return left_; }

private:
    std::vector<assets::TargetPoint> left_;
};

}
