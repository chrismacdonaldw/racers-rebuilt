#include "sim/physics/facing.hpp"

#include <algorithm>
#include <cmath>

namespace rr::sim {
namespace {

constexpr float kMinFacingSpeed = 0.03F;
constexpr float kSnapDot = 0.98F;
constexpr float kSlewRatePerMs = 0.0025F;

float AngleBetween(const Vector3& a, const Vector3& b) {
    return std::acos(std::clamp(Dot(a, b), -1.0F, 1.0F));
}

Vector3 RotateAbout(const Vector3& v, const Vector3& axis, float radians) {
    const float cos = std::cos(radians);
    return (v * cos) + (Cross(axis, v) * std::sin(radians)) +
           (axis * (Dot(axis, v) * (1.0F - cos)));
}

Vector3 SlewToward(const Vector3& facing, const Vector3& target, int dtMs) {
    if (Dot(facing, target) > kSnapDot) {
        return target;
    }
    const float step = kSlewRatePerMs * static_cast<float>(dtMs);
    if (step >= AngleBetween(facing, target)) {
        return target;
    }
    const Vector3 axis = Normalized(Cross(facing, target), {.z = 1.0F});
    return Normalized(RotateAbout(facing, axis, step), target);
}

Vector3 TurnWithLag(const BodyState& body, int dtMs) {
    const Vector3& forward = body.motion.basis.forward;
    const float turn = body.yawRate * static_cast<float>(dtMs);
    if (AngleBetween(body.facing, forward) + std::abs(turn) > body.steering.lagMaximum) {
        const Vector3 trailing{.z = body.yawRate > 0.0F ? -1.0F : 1.0F};
        const Vector3 axis = Normalized(Cross(forward, body.facing), trailing);
        return RotateAbout(forward, axis, body.steering.lagMaximum);
    }
    const float x = (body.facing.x * std::cos(turn)) - (body.facing.y * std::sin(turn));
    const float y = (x * std::sin(turn)) + (body.facing.y * std::cos(turn));
    return Normalized({.x = x, .y = y, .z = body.facing.z}, forward);
}

Vector3 ProjectOntoPlane(const Vector3& v, const Vector3& normal, const Vector3& fallback) {
    return Normalized(v - (normal * Dot(v, normal)), fallback);
}

}

void UpdateFacing(BodyState& body, int dtMs) {
    const BodyMotion& motion = body.motion;
    const Vector3& forward = motion.basis.forward;
    const bool isMoving = IsGrounded(motion) && body.derived.forwardSpeed >= kMinFacingSpeed;
    if (isMoving && motion.contactCount <= 2) {
        const Vector3 target = ProjectOntoPlane(forward, motion.supportNormal, forward);
        body.facing = SlewToward(body.facing, target, dtMs);
    } else if (isMoving && body.steering.isSteering && body.thrust > 0.0F) {
        body.facing = TurnWithLag(body, dtMs);
    } else {
        body.facing = SlewToward(body.facing, forward, dtMs);
    }
}

void UpdateDerivedMotion(BodyState& body) {
    const Vector3& velocity = body.motion.velocity;
    DerivedMotion& derived = body.derived;
    derived.speed = Length(velocity);
    derived.velocityDirection = Normalized(velocity, body.motion.basis.forward);
    derived.isAtRest = velocity == Vector3{} && body.angularVelocity == Vector3{};
    derived.forwardSpeed = Dot(velocity, body.facing);
    derived.forwardVelocity = body.facing * derived.forwardSpeed;
    derived.lateralVelocity = velocity - derived.forwardVelocity;
}

}
