#include "sim/physics/facing.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace rr::sim {
namespace {

constexpr float kMinFacingSpeed = 0.03F;
constexpr float kSnapDot = 0.98F;
constexpr float kDriftRollGain = 0.06F;
constexpr std::size_t kArccosTableSize = 1024;
constexpr float kArccosTableScale = static_cast<float>(kArccosTableSize - 1) / 2.0F;

const std::array<float, kArccosTableSize>& ArccosTable() {
    static const std::array<float, kArccosTableSize> table = [] {
        std::array<float, kArccosTableSize> angles{};
        for (std::size_t i = 0; i < kArccosTableSize; ++i) {
            angles.at(i) =
                static_cast<float>(std::acos((static_cast<double>(i) / kArccosTableScale) - 1.0));
        }
        return angles;
    }();
    return table;
}

float AngleBetween(const Vector3& a, const Vector3& b) {
    const float cosine = std::clamp(Dot(a, b), -1.0F, 1.0F);
    return ArccosTable().at(static_cast<std::size_t>((cosine + 1.0F) * kArccosTableScale));
}

Vector3 SlewToward(const Vector3& facing, const Vector3& target, float ratePerMs, int dtMs) {
    if (Dot(facing, target) > kSnapDot) {
        return target;
    }
    const float step = ratePerMs * static_cast<float>(dtMs);
    if (step > AngleBetween(facing, target)) {
        return target;
    }
    return Normalized(RotateAbout(facing, Cross(facing, target), step), target);
}

Vector3 TurnWithLag(const BodyState& body, int dtMs) {
    const Vector3& forward = body.motion.basis.forward;
    const float turn = body.yawRate * static_cast<float>(dtMs);
    if (AngleBetween(body.facing, forward) + std::abs(turn) > body.steering.lagMaximum) {
        return Normalized(
            RotateAbout(forward, Cross(forward, body.facing), body.steering.lagMaximum), forward);
    }
    const float x = (body.facing.x * std::cos(turn)) - (body.facing.y * std::sin(turn));
    const float y = (x * std::sin(turn)) + (body.facing.y * std::cos(turn));
    return Normalized({.x = x, .y = y, .z = body.facing.z}, forward);
}

Vector3 ProjectOntoPlane(const Vector3& v, const Vector3& normal, const Vector3& fallback) {
    return Normalized(v - (normal * Dot(v, normal)), fallback);
}

}

void UpdateFacing(BodyState& body, float slewRatePerMs, int dtMs) {
    const BodyMotion& motion = body.motion;
    const Vector3& forward = motion.basis.forward;
    const bool isMoving = IsGrounded(motion) && body.derived.forwardSpeed >= kMinFacingSpeed;
    if (isMoving && motion.contactCount <= 2) {
        const Vector3 target = ProjectOntoPlane(forward, motion.supportNormal, forward);
        body.facing = SlewToward(body.facing, target, slewRatePerMs, dtMs);
    } else if (isMoving && body.steering.isSteering && body.thrust > 0.0F) {
        body.facing = TurnWithLag(body, dtMs);
    } else {
        body.facing = SlewToward(body.facing, forward, slewRatePerMs, dtMs);
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

float DriftRoll(const BodyState& body, bool isDrifting, const VersionRules& rules) {
    if (!isDrifting || rules.dropsDriftLean) {
        return 0.0F;
    }
    const float trail = 1.0F - Dot(body.facing, body.motion.basis.forward);
    return (body.turnRadius < 0.0F ? -trail : trail) * kDriftRollGain;
}

}
