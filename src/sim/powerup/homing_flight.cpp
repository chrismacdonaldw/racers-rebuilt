#include "sim/powerup/homing_flight.hpp"

#include <algorithm>
#include <cmath>

#include "sim/powerup/aim_cones.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kRetargetTime{1000};
constexpr float kSnapDistanceSquared = 3600.0F;
constexpr float kDirectHitDistanceSquared = 9.0F;
constexpr float kPathReachSquared = 90'000.0F;
constexpr float kGatePassMargin = 10.0F;
constexpr Vector3 kGroundProbeAbove{.z = 2.0F};
constexpr float kGroundClearance = 6.0F;
constexpr float kDescentPerSecond = 6.0F;
constexpr Vector3 kWaypointProbeAbove{.z = 5.0F};
constexpr Vector3 kWaypointProbeBelow{.z = -50.0F};
constexpr float kSpiralGrowthPerSecond = 6.0F;
constexpr float kSpiralAmplitude = 4.0F;
constexpr float kSpiralRadiansPerSecond = 10.0F;
constexpr Vector3 kUp{.z = 1.0F};

bool CanTarget(const Racer& racer) {
    return !racer.IsSpunOut() && !racer.IsShielded() && !racer.Status().isGhost;
}

std::optional<std::size_t> FindTarget(std::span<const Racer> racers, std::size_t owner,
                                      const Vector3& origin, const Vector3& direction) {
    for (std::optional<std::size_t> found =
             FindRacerInCone(racers, origin, direction, kMissileCone);
         found; found = FindRacerInCone(racers, origin, direction, kMissileCone, *found + 1)) {
        if (*found != owner && CanTarget(racers[*found])) {
            return found;
        }
    }
    return std::nullopt;
}

}

void HomingFlight::Launch(const HomingLaunch& launch) {
    position_ = launch.start;
    drawn_ = launch.start;
    direction_ = launch.direction;
    hitNormal_ = {};
    speed_ = launch.speed;
    snapSpeed_ = launch.snapSpeed;
    spiralAmplitude_ = 0.0F;
    spiralAngle_ = 0.0F;
    age_ = Milliseconds{0};
    lifetime_ = launch.lifetime;
    sinceRetarget_ = Milliseconds{0};
    target_ = launch.target;
    pathGate_ = launch.pathGate;
    waypoint_.reset();
}

void HomingFlight::Steer(Milliseconds elapsed, std::span<const Racer> racers, std::size_t owner,
                         const CheckpointGraph& checkpoints, const CollisionQuery& world) {
    sinceRetarget_ += elapsed;
    if (!target_ || sinceRetarget_ > kRetargetTime || !CanTarget(racers[*target_])) {
        sinceRetarget_ = Milliseconds{0};
        target_ = FindTarget(racers, owner, position_, direction_);
    }
    if (!waypoint_ && pathGate_) {
        FollowGates(checkpoints, world);
    }
}

FlightState HomingFlight::Step(Milliseconds elapsed, std::span<const Racer> racers,
                               const CollisionQuery& ground, const CollisionQuery& shots) {
    age_ += elapsed;
    if (age_ >= lifetime_) {
        return FlightState::Expired;
    }
    const float seconds = static_cast<float>(elapsed.count()) * 0.001F;
    std::optional<Vector3> next;
    if (target_) {
        const Vector3 aim = AimPoint(racers[*target_]);
        direction_ = aim - position_;
        if (Dot(direction_, direction_) < kSnapDistanceSquared) {
            next = Close(aim, seconds);
            if (!next) {
                drawn_ = aim;
                return FlightState::ReachedTarget;
            }
        }
    }
    position_ = next ? *next : Cruise(seconds, ground);
    const Vector3 drawn = position_ + Spiral(seconds);
    if (const std::optional<SegmentHit> hit = CastSolid(shots, drawn_, drawn)) {
        drawn_ = hit->point;
        hitNormal_ = hit->normal;
        return FlightState::HitWorld;
    }
    drawn_ = drawn;
    return FlightState::Flying;
}

void HomingFlight::TurnBack(std::optional<std::size_t> target) {
    age_ = Milliseconds{0};
    target_ = target;
}

void HomingFlight::FollowGates(const CheckpointGraph& checkpoints, const CollisionQuery& world) {
    const std::span<const assets::Checkpoint> gates = checkpoints.Checkpoints();
    std::size_t gate = *pathGate_;
    for (std::size_t step = 0;; ++step) {
        const assets::Checkpoint& current = gates[gate];
        if (Dot(current.position - position_, direction_) > 0.0F &&
            SignedDistance(current, position_) >= kGatePassMargin) {
            break;
        }
        gate = current.links[0];
        const Vector3 toNext = gates[gate].position - position_;
        if (step == gates.size() || Dot(toNext, toNext) > kPathReachSquared ||
            Dot(gates[gate].normal, direction_) > 0.0F) {
            pathGate_.reset();
            return;
        }
    }
    pathGate_ = gate;
    const Vector3& point = gates[gate].position;
    if (const std::optional<SegmentHit> ground =
            CastSolid(world, point + kWaypointProbeAbove, point + kWaypointProbeBelow)) {
        waypoint_ = ground->point + Vector3{.z = kGroundClearance};
    }
}

std::optional<Vector3> HomingFlight::Close(const Vector3& aim, float seconds) {
    const float distanceSquared = Dot(direction_, direction_);
    direction_ = Normalized(direction_, {.x = 1.0F});
    if (distanceSquared < kDirectHitDistanceSquared) {
        return std::nullopt;
    }
    const Vector3 next = position_ + (direction_ * (snapSpeed_ * seconds));
    if (Dot(aim - next, direction_) <= 0.0F) {
        return std::nullopt;
    }
    return next;
}

Vector3 HomingFlight::Cruise(float seconds, const CollisionQuery& world) {
    if (waypoint_) {
        const Vector3 toWaypoint = *waypoint_ - position_;
        if (Dot(direction_, toWaypoint) > 0.0F) {
            direction_ = toWaypoint;
        } else {
            waypoint_.reset();
        }
    }
    direction_ = Normalized(direction_, {.x = 1.0F});
    Vector3 next = position_ + (direction_ * (speed_ * seconds));
    if (const std::optional<SegmentHit> ground =
            CastSolid(world, next + kGroundProbeAbove, next - Vector3{.z = kGroundClearance})) {
        const float hover = ground->point.z + kGroundClearance;
        next.z = position_.z > hover ? std::max(hover, position_.z - (kDescentPerSecond * seconds))
                                     : hover;
    }
    return next;
}

Vector3 HomingFlight::Spiral(float seconds) {
    spiralAmplitude_ =
        std::min(spiralAmplitude_ + (kSpiralGrowthPerSecond * seconds), kSpiralAmplitude);
    spiralAngle_ += kSpiralRadiansPerSecond * seconds;
    return RotateAbout(kUp, direction_, -spiralAngle_) * spiralAmplitude_;
}

}
