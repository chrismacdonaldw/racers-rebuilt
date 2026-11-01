#pragma once

#include "core/quaternion.hpp"
#include "core/vector3.hpp"
#include "sim/physics/body_state.hpp"
#include "sim/physics/chassis.hpp"

namespace rr::sim {

[[nodiscard]] Vector3 ToBody(const Basis& basis, const Vector3& world);

[[nodiscard]] Vector3 ToWorld(const Basis& basis, const Vector3& local);

[[nodiscard]] Basis OrthonormalBasis(const Vector3& forward, const Vector3& up);

[[nodiscard]] Quaternion ToQuaternion(const Basis& basis);

[[nodiscard]] Basis ToBasis(const Quaternion& rotation);

[[nodiscard]] Vector3 CentreOfMassWorld(const BodyMotion& motion, const BodyConstants& constants);

void ApplyForceAt(BodyState& body, const BodyConstants& constants, const Vector3& force,
                  const Vector3& point);

void CancelAngularMomentum(BodyMotion& motion, const Vector3& axis);

/// `ratePerMs` is in radians per ms about the body's up axis.
void SetYawImpulse(BodyState& body, const BodyConstants& constants, float ratePerMs,
                   int durationMs);

/// `ratePerMs` is in radians per ms about the body's side axis. Ignored during a roll impulse.
void SetPitchImpulse(BodyState& body, const BodyConstants& constants, float ratePerMs,
                     int durationMs);

void Integrate(BodyState& body, const BodyConstants& constants, int dtMs);

void LimitUprightTilt(BodyMotion& motion);

}
