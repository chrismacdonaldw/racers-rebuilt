#pragma once

#include <cstdint>

#include "core/duration.hpp"
#include "core/vector3.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/racer.hpp"

namespace rr::sim {

/// `speed` is in units per s and `gravity` in units per s² along z.
struct BallisticSpec {
    float speed = 0.0F;
    float gravity = 0.0F;
    Milliseconds lifetime{0};
    bool leadsOnlyBehind = false;
};

enum class FlightState : std::uint8_t { Flying, HitWorld, Expired, ReachedTarget };

/// Keeps falling past its aim point until it hits something or runs out.
class BallisticFlight {
public:
    /// Leads `target` by its velocity over the flight, adding the owner's speed along the aim.
    void LaunchAtRacer(const BallisticSpec& spec, const Racer& owner, const Vector3& start,
                       const Racer& target);

    /// Adds the owner's speed only when the owner moves toward `aim`.
    void LaunchAtPoint(const BallisticSpec& spec, const Racer& owner, const Vector3& start,
                       const Vector3& aim);

    /// Relaunches from here at its speed, leading `target`, or back to the start without one.
    void TurnBack(const Racer* target);

    [[nodiscard]] FlightState Step(Milliseconds elapsed, const CollisionQuery& world);

    [[nodiscard]] const Vector3& Position() const { return position_; }

    /// In units per s.
    [[nodiscard]] Vector3 Velocity() const;

    [[nodiscard]] float Speed() const { return speed_; }
    [[nodiscard]] const Vector3& HitNormal() const { return hitNormal_; }
    [[nodiscard]] Milliseconds Remaining() const { return lifetime_ - age_; }

private:
    void Aim(const Vector3& start, const Vector3& aim, float seconds);

    Vector3 start_;
    Vector3 velocity_;
    Vector3 position_;
    Vector3 hitNormal_;
    float gravity_ = 0.0F;
    float speed_ = 0.0F;
    Milliseconds age_{0};
    Milliseconds lifetime_{0};
};

/// `target`'s position plus the 5 units weapons aim above a car.
[[nodiscard]] Vector3 AimPoint(const Racer& target);

}
