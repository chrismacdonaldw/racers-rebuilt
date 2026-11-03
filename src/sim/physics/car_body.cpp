#include "sim/physics/car_body.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "sim/physics/body_effects.hpp"
#include "sim/physics/body_forces.hpp"
#include "sim/physics/facing.hpp"
#include "sim/physics/ground_contact.hpp"
#include "sim/physics/hover_contact.hpp"
#include "sim/physics/rigid_body.hpp"
#include "sim/physics/wall_sweep.hpp"

namespace rr::sim {
namespace {

constexpr float kMinTurnRadius = 40.0F;
constexpr float kMaxTurnRadius = 4096.0F;
constexpr int kWallRetryMarginMs = 5;
constexpr int kMaxWallPasses = 4;

bool CountDown(int& timerMs, int dtMs) {
    if (timerMs <= 0) {
        return false;
    }
    timerMs -= dtMs;
    if (timerMs > 0) {
        return false;
    }
    timerMs = 0;
    return true;
}

int RunTimers(BodyState& body, const BodyConstants& constants, int dtMs) {
    body.hasWallContact = false;
    CountDownSpinOut(body, dtMs);
    const Basis& basis = body.motion.basis;
    if (body.spin.isSpinning) {
        dtMs = CountDownSpin(body, dtMs);
    } else if (CountDown(body.impulses.yawMs, dtMs)) {
        CancelAngularMomentum(body.motion, basis.up);
        body.hasWallYaw = false;
    }
    body.spin.stopAtMs = std::max(body.spin.stopAtMs - dtMs, 0);
    if (CountDown(body.impulses.rollMs, dtMs)) {
        CancelAngularMomentum(body.motion, basis.forward);
    }
    if (CountDown(body.impulses.pitchMs, dtMs)) {
        CancelAngularMomentum(body.motion, basis.side);
    }
    if (body.hover.isHovering) {
        body.hover.lift.targetBank = HoverBankTarget(body.turnRadius, constants.rules);
    }
    return dtMs;
}

void UpdateContacts(BodyState& body, const BodyConstants& constants, int dtMs,
                    const CollisionQuery& world) {
    if (body.hover.isHovering) {
        UpdateHoverLift(body.hover.lift, dtMs);
        UpdateHoverContact(body, constants, dtMs, world);
    } else {
        UpdateGroundContact(body, constants, dtMs, world);
    }
}

void RunPass(BodyState& body, const BodyConstants& constants, int dtMs,
             const CollisionQuery& world) {
    AccumulateForces(body, constants);
    Integrate(body, constants, dtMs);
    LimitUprightTilt(body.motion);
    UpdateContacts(body, constants, dtMs, world);
    UpdateFacing(body, constants.rules.facingRatePerMs, dtMs);
    UpdateDerivedMotion(body);
    UpdateWheelSurfaces(body.motion, constants.ignoresSurfaces);
}

int RetryStepMs(int stepMs, float fraction) {
    const int reachedMs = static_cast<int>(std::floor(static_cast<float>(stepMs) * fraction));
    return std::max(reachedMs - kWallRetryMarginMs, 0);
}

void RespondToWall(BodyState& body, const BodyConstants& constants, const Vector3& normal) {
    ApplyWallResponse(body, constants, normal);
    body.hasWallContact = true;
    UpdateDerivedMotion(body);
    ClearContacts(body.motion);
    body.saved = body.motion;
}

void RunPassesAgainstWalls(BodyState& body, const BodyConstants& constants, int dtMs,
                           const CollisionQuery& world, SurfaceTouches* touches) {
    int remainingMs = dtMs;
    int stepMs = dtMs;
    std::optional<Vector3> wallNormal;
    for (int pass = 1; pass <= kMaxWallPasses; ++pass) {
        const bool isLastPass = pass == kMaxWallPasses;
        if (stepMs > 0) {
            RunPass(body, constants, stepMs, world);
            if (const std::optional<WallHit> hit =
                    SweepBodyPoints(body, constants, world, touches)) {
                body.motion = body.saved;
                wallNormal = hit->normal;
                stepMs = isLastPass ? 0 : RetryStepMs(stepMs, hit->fraction);
                if (!isLastPass) {
                    continue;
                }
            }
        }
        if (!wallNormal) {
            return;
        }
        RespondToWall(body, constants, *wallNormal);
        wallNormal.reset();
        remainingMs -= stepMs;
        stepMs = remainingMs;
        if (remainingMs <= 0) {
            return;
        }
    }
}

}

BodyState MakeBody(const BodyConstants& constants, const Vector3& position, const Basis& basis) {
    BodyState body;
    body.motion.position = position;
    body.motion.basis = basis;
    body.facing = basis.forward;
    PlaceWheels(body.motion, constants);
    body.saved = body.motion;
    UpdateDerivedMotion(body);
    return body;
}

void SetDrive(BodyState& body, const BodyConstants& constants, float thrust, float maxSpeed) {
    body.thrust = thrust;
    body.maxSpeed = maxSpeed;
    body.dragCoefficient =
        thrust == 0.0F ? 0.0F : constants.mass * std::abs(thrust) / (maxSpeed * maxSpeed);
}

void SetTurnRadius(BodyState& body, float radius) {
    const float magnitude = std::abs(radius);
    if (magnitude > kMaxTurnRadius) {
        body.turnRadius = 0.0F;
    } else if (magnitude > 0.0F && magnitude < kMinTurnRadius) {
        body.turnRadius = std::copysign(kMinTurnRadius, radius);
    } else {
        body.turnRadius = radius;
    }
}

void StepBody(BodyState& body, const BodyConstants& constants, int dtMs,
              const CollisionQuery& world, SurfaceTouches* touches) {
    const int stepMs = RunTimers(body, constants, dtMs);
    body.saved = body.motion;
    RunPassesAgainstWalls(body, constants, stepMs, world, touches);
    body.airborneMs = IsGrounded(body.motion) ? 0 : body.airborneMs + dtMs;
}

}
