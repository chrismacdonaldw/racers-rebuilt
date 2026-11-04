#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "sim/ai/racing_line.hpp"
#include "sim/physics/racer.hpp"

namespace rr::sim {

/// Speeds in units per s, accelerations in units per s².
struct CarLimits {
    float topSpeed = 0.0F;
    float thrust = 0.0F;
    float brake = 0.0F;
    float cornerGrip = 0.0F;
    float lockRadius = 0.0F;

    /// Grip is read from the surface under the car, so it should be on the ground.
    [[nodiscard]] static CarLimits Of(const Racer& racer);
};

/// The fastest speed at each point of a racing line that a car can corner at and brake down to.
class SpeedPlan {
public:
    SpeedPlan(const RacingLine& line, const CarLimits& car);

    /// In units per s. `target` and `alpha` place a car as `RoutePlayback` does.
    [[nodiscard]] float SpeedAt(std::size_t target, float alpha) const;

    [[nodiscard]] std::span<const float> Speeds() const { return speeds_; }

    /// What the car's engine can reach from a standing start, then lap after lap.
    [[nodiscard]] std::span<const float> Reachable() const { return reachable_; }

    /// One lap of the line's loop at the reachable speeds.
    [[nodiscard]] float LapSeconds() const { return lapSeconds_; }

    /// Where drifting corners faster than grip turning can.
    [[nodiscard]] bool IsDriftAt(std::size_t point) const {
        return point < drifts_.size() && drifts_[point];
    }

private:
    std::vector<float> speeds_;
    std::vector<float> reachable_;
    std::vector<bool> drifts_;
    float lapSeconds_ = 0.0F;
};

}
