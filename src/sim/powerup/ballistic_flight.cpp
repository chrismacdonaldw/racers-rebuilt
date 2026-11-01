#include "sim/powerup/ballistic_flight.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace rr::sim {
namespace {

constexpr float kMinSpeed = 0.001F;
constexpr float kMsPerSecond = 1000.0F;
constexpr Vector3 kAimHeight{.z = 5.0F};

Vector3 Horizontal(Vector3 v) {
    v.z = 0.0F;
    return v;
}

float HorizontalDistance(const Vector3& from, const Vector3& to) {
    const float distance = Length(Horizontal(to - from));
    return distance == 0.0F ? 1.0F : distance;
}

float OwnerSpeedAlong(const Racer& owner, const Vector3& from, const Vector3& to) {
    const Vector3 direction = Normalized(Horizontal(to - from), {});
    const DerivedMotion& motion = owner.Body().derived;
    return Dot(motion.velocityDirection, direction) * motion.speed * kMsPerSecond;
}

float Seconds(Milliseconds time) {
    return static_cast<float>(time.count()) / kMsPerSecond;
}

}

Vector3 AimPoint(const Racer& target) {
    return target.Body().motion.position + kAimHeight;
}

void BallisticFlight::LaunchAtRacer(const BallisticSpec& spec, const Racer& owner,
                                    const Vector3& start, const Racer& target) {
    gravity_ = spec.gravity;
    lifetime_ = spec.lifetime;
    const Vector3 at = AimPoint(target);
    const float speed =
        std::max(spec.speed + std::abs(OwnerSpeedAlong(owner, start, at)), kMinSpeed);
    const float seconds = HorizontalDistance(start, at) / speed;
    Aim(start, at + (target.Body().motion.velocity * (seconds * kMsPerSecond)), seconds);
}

void BallisticFlight::LaunchAtPoint(const BallisticSpec& spec, const Racer& owner,
                                    const Vector3& start, const Vector3& aim) {
    gravity_ = spec.gravity;
    lifetime_ = spec.lifetime;
    const float speed = spec.speed + std::max(OwnerSpeedAlong(owner, start, aim), 0.0F);
    Aim(start, aim, HorizontalDistance(start, aim) / speed);
}

void BallisticFlight::TurnBack(const Racer* target) {
    const Vector3 from = position_;
    const Vector3 at = target != nullptr ? AimPoint(*target) : start_;
    const float seconds = HorizontalDistance(from, at) / std::max(speed_, kMinSpeed);
    const Vector3 lead =
        target != nullptr ? target->Body().motion.velocity * (seconds * kMsPerSecond) : Vector3{};
    Aim(from, at + lead, seconds);
}

FlightState BallisticFlight::Step(Milliseconds elapsed, const CollisionQuery& world) {
    age_ += elapsed;
    if (age_ >= lifetime_) {
        return FlightState::Expired;
    }
    const float t = Seconds(age_);
    const Vector3 next = start_ + (velocity_ * t) + Vector3{.z = 0.5F * gravity_ * t * t};
    if (const std::optional<SegmentHit> hit = CastSolid(world, position_, next)) {
        position_ = hit->point;
        hitNormal_ = hit->normal;
        return FlightState::HitWorld;
    }
    position_ = next;
    return FlightState::Flying;
}

Vector3 BallisticFlight::Velocity() const {
    return velocity_ + Vector3{.z = gravity_ * Seconds(age_)};
}

void BallisticFlight::Aim(const Vector3& start, const Vector3& aim, float seconds) {
    const Vector3 delta = aim - start;
    start_ = start;
    position_ = start;
    hitNormal_ = {};
    age_ = Milliseconds{0};
    speed_ = Length(Horizontal(delta)) / seconds;
    velocity_ = Horizontal(delta) * (1.0F / seconds);
    velocity_.z = (delta.z / seconds) - (0.5F * gravity_ * seconds);
}

}
