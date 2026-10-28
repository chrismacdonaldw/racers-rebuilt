#include "sim/physics/rigid_body.hpp"

#include <cmath>
#include <numbers>

namespace rr::sim {
namespace {

constexpr float kRestVelocity = 0.0001F;
constexpr float kRestRotationSquared = 0.0006F;
constexpr float kMinUpZ = std::numbers::sqrt2_v<float> / 2.0F;

Vector3 ToBody(const Basis& basis, const Vector3& world) {
    return {.x = Dot(basis.forward, world), .y = Dot(basis.side, world), .z = Dot(basis.up, world)};
}

Vector3 AngularVelocity(const BodyMotion& motion, const BodyConstants& constants) {
    const Vector3 local = ToBody(motion.basis, motion.angularMomentum);
    return ToWorld(motion.basis, {.x = local.x / constants.inertia.x,
                                  .y = local.y / constants.inertia.y,
                                  .z = local.z / constants.inertia.z});
}

void ZeroSlowComponents(Vector3& velocity) {
    for (float* component : {&velocity.x, &velocity.y, &velocity.z}) {
        if (std::abs(*component) < kRestVelocity) {
            *component = 0.0F;
        }
    }
}

void Rotate(Basis& basis, const Vector3& rotation) {
    const Vector3 forward = basis.forward + Cross(rotation, basis.forward);
    const Vector3 up = basis.up + Cross(rotation, basis.up);
    basis = OrthonormalBasis(forward, up);
}

}

Vector3 ToWorld(const Basis& basis, const Vector3& local) {
    return (basis.forward * local.x) + (basis.side * local.y) + (basis.up * local.z);
}

Basis OrthonormalBasis(const Vector3& forward, const Vector3& up) {
    const Vector3 unitForward = Normalized(forward, {.x = 1.0F});
    const Vector3 side = Normalized(Cross(up, unitForward), {.y = 1.0F});
    return {.forward = unitForward, .side = side, .up = Cross(unitForward, side)};
}

Quaternion ToQuaternion(const Basis& basis) {
    const Vector3& f = basis.forward;
    const Vector3& s = basis.side;
    const Vector3& u = basis.up;
    const float trace = f.x + s.y + u.z;
    if (trace > 0.0F) {
        const float root = std::sqrt(trace + 1.0F) * 2.0F;
        return {.w = root / 4.0F,
                .x = (s.z - u.y) / root,
                .y = (u.x - f.z) / root,
                .z = (f.y - s.x) / root};
    }
    if (f.x > s.y && f.x > u.z) {
        const float root = std::sqrt(1.0F + f.x - s.y - u.z) * 2.0F;
        return {.w = (s.z - u.y) / root,
                .x = root / 4.0F,
                .y = (s.x + f.y) / root,
                .z = (u.x + f.z) / root};
    }
    if (s.y > u.z) {
        const float root = std::sqrt(1.0F + s.y - f.x - u.z) * 2.0F;
        return {.w = (u.x - f.z) / root,
                .x = (s.x + f.y) / root,
                .y = root / 4.0F,
                .z = (u.y + s.z) / root};
    }
    const float root = std::sqrt(1.0F + u.z - f.x - s.y) * 2.0F;
    return {.w = (f.y - s.x) / root,
            .x = (u.x + f.z) / root,
            .y = (u.y + s.z) / root,
            .z = root / 4.0F};
}

Vector3 CentreOfMassWorld(const BodyMotion& motion, const BodyConstants& constants) {
    return motion.position + ToWorld(motion.basis, constants.centreOfMass);
}

void ApplyForceAt(BodyState& body, const BodyConstants& constants, const Vector3& force,
                  const Vector3& point) {
    body.force += force;
    body.torque += Cross(point - CentreOfMassWorld(body.motion, constants), force);
}

void CancelAngularMomentum(BodyMotion& motion, const Vector3& axis) {
    motion.angularMomentum -= axis * Dot(motion.angularMomentum, axis);
}

void SetYawImpulse(BodyState& body, const BodyConstants& constants, float ratePerMs,
                   int durationMs) {
    body.impulses.yawMs = durationMs;
    CancelAngularMomentum(body.motion, body.motion.basis.up);
    body.motion.angularMomentum += body.motion.basis.up * (constants.inertia.z * ratePerMs);
}

void Integrate(BodyState& body, const BodyConstants& constants, int dtMs) {
    BodyMotion& motion = body.motion;
    const auto dt = static_cast<float>(dtMs);
    body.angularVelocity = AngularVelocity(motion, constants);
    const Vector3 deltaVelocity = body.force * (dt / constants.mass);
    motion.position += (motion.velocity * dt) + (deltaVelocity * (0.5F * dt));
    if (deltaVelocity == Vector3{}) {
        ZeroSlowComponents(motion.velocity);
    }
    motion.velocity += deltaVelocity;
    const Vector3 rotation = body.angularVelocity * dt;
    Rotate(motion.basis, rotation);
    motion.angularMomentum += body.torque * dt;
    if (body.torque == Vector3{} && Dot(rotation, rotation) < kRestRotationSquared) {
        body.angularVelocity = {};
        motion.angularMomentum = {};
    }
    body.force = {};
    body.torque = {};
}

void LimitUprightTilt(BodyMotion& motion) {
    const Vector3& up = motion.basis.up;
    if (up.z >= kMinUpZ) {
        return;
    }
    const Vector3 lean = Normalized({.x = up.x, .y = up.y}, {.x = 1.0F});
    const Vector3 limited = (lean * kMinUpZ) + Vector3{.z = kMinUpZ};
    motion.basis = OrthonormalBasis(motion.basis.forward, limited);
}

}
