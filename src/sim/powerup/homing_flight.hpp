#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include "core/duration.hpp"
#include "core/vector3.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/racer.hpp"
#include "sim/powerup/ballistic_flight.hpp"
#include "sim/race/checkpoint_graph.hpp"

namespace rr::sim {

/// Speeds are in units per s. `snapSpeed` takes over within 60 units of the target.
struct HomingLaunch {
    Vector3 start;
    Vector3 direction;
    float speed = 0.0F;
    float snapSpeed = 0.0F;
    Milliseconds lifetime{0};
    std::optional<std::size_t> target;
    std::optional<std::size_t> pathGate;
};

class HomingFlight {
public:
    void Launch(const HomingLaunch& launch);

    /// Runs before each `Step`. Retargets every second, or when the target can't be hit.
    void Steer(Milliseconds elapsed, std::span<const Racer> racers, std::size_t owner,
               const CheckpointGraph& checkpoints, const CollisionQuery& world);

    /// `ground` carries the hover, and `shots` stops the drawn path.
    [[nodiscard]] FlightState Step(Milliseconds elapsed, std::span<const Racer> racers,
                                   const CollisionQuery& ground, const CollisionQuery& shots);

    void TurnBack(std::optional<std::size_t> target);

    /// Includes the spiral around the flight direction.
    [[nodiscard]] const Vector3& Position() const { return drawn_; }

    [[nodiscard]] const Vector3& Direction() const { return direction_; }
    [[nodiscard]] const Vector3& HitNormal() const { return hitNormal_; }
    [[nodiscard]] std::optional<std::size_t> Target() const { return target_; }
    [[nodiscard]] Milliseconds Remaining() const { return lifetime_ - age_; }

private:
    void FollowGates(const CheckpointGraph& checkpoints, const CollisionQuery& world);
    [[nodiscard]] std::optional<Vector3> Close(const Vector3& aim, float seconds);
    [[nodiscard]] Vector3 Cruise(float seconds, const CollisionQuery& world);
    [[nodiscard]] Vector3 Spiral(float seconds);

    Vector3 position_;
    Vector3 drawn_;
    Vector3 direction_{.x = 1.0F};
    Vector3 hitNormal_;
    float speed_ = 0.0F;
    float snapSpeed_ = 0.0F;
    float spiralAmplitude_ = 0.0F;
    float spiralAngle_ = 0.0F;
    Milliseconds age_{0};
    Milliseconds lifetime_{0};
    Milliseconds sinceRetarget_{0};
    std::optional<std::size_t> target_;
    std::optional<std::size_t> pathGate_;
    std::optional<Vector3> waypoint_;
};

}
