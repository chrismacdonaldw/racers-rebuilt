#include "sim/physics/body_forces.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr int kYawImpulseMs = 200;
constexpr float kAirborneGravityScale = 4.0F;
constexpr float kGripDamping = 0.01F;
constexpr float kWallYawDamping = 0.002F;
constexpr float kCoastDamping = 0.001F;
constexpr float kCrawlMinSpeed = 0.0005F;
constexpr float kCrawlTurnSpeed = 0.03F;
constexpr float kWheelShare = 0.25F;

float GravityForce(const BodyConstants& constants) {
    return -constants.gravity * constants.massScale;
}

float AverageOverWheels(const BodyMotion& motion, float Surface::* value) {
    float total = 0.0F;
    for (const WheelProbe& wheel : motion.wheels) {
        total += wheel.surface.*value;
    }
    return total * kWheelShare;
}

void AddAirborneForces(BodyState& body, const BodyConstants& constants) {
    const float gravity = GravityForce(constants);
    body.force += Vector3{.z = kAirborneGravityScale * gravity};
    Vector3 thrust = body.facing * (body.thrust * constants.massScale);
    thrust.z = std::min(thrust.z, -gravity);
    body.force += thrust;
    if (body.turnRadius != 0.0F && !body.hasWallYaw) {
        SetYawImpulse(body, constants, body.derived.forwardSpeed / body.turnRadius, kYawImpulseMs);
    }
}

void CancelTipping(BodyMotion& motion, const Vector3& contact, const Vector3& opposite) {
    const Vector3 axis = Normalized(Cross(opposite - contact, motion.supportNormal), {});
    const float toward = Dot(motion.angularMomentum, axis);
    if (toward > 0.0F) {
        motion.angularMomentum -= axis * toward;
    }
}

void AddWheelForces(BodyState& body, const BodyConstants& constants) {
    BodyMotion& motion = body.motion;
    for (std::size_t i = 0; i < kWheelCount; ++i) {
        const WheelProbe& wheel = motion.wheels[i];
        if (!wheel.isInContact) {
            continue;
        }
        if (motion.contactCount < 3) {
            ApplyForceAt(body, constants, wheel.contactForce, wheel.position);
        }
        CancelTipping(motion, wheel.position, motion.wheels[kWheelCount - 1 - i].position);
    }
}

void AddHoverForces(BodyState& body) {
    BodyMotion& motion = body.motion;
    const HoverState& hover = body.hover;
    if (motion.wheels[0].isInContact) {
        CancelTipping(motion, hover.frontAxle, hover.rearAxle);
    }
    if (motion.wheels[2].isInContact) {
        CancelTipping(motion, hover.rearAxle, hover.frontAxle);
    }
}

float YawImpulseRate(const BodyState& body) {
    const float forwardSpeed = body.derived.forwardSpeed;
    if (body.steering.isSteering) {
        const float alignment = Dot(body.motion.basis.forward, body.derived.velocityDirection);
        return alignment >= body.steering.alignmentMinimum
                   ? forwardSpeed / body.turnRadius * body.steering.gain
                   : 0.0F;
    }
    if (forwardSpeed > kCrawlMinSpeed && forwardSpeed < kCrawlTurnSpeed) {
        return kCrawlTurnSpeed / body.turnRadius;
    }
    return forwardSpeed / body.turnRadius;
}

void AddGripForces(BodyState& body, const BodyConstants& constants, float contactScale) {
    const DerivedMotion& derived = body.derived;
    body.force += derived.lateralVelocity * (-kGripDamping * constants.mass * contactScale);
    if (body.thrust != 0.0F) {
        const bool alongBody = body.motion.contactCount >= 3 && !body.steering.isSteering;
        const Vector3& direction = alongBody ? body.motion.basis.forward : body.facing;
        body.force += direction * (body.thrust * constants.massScale);
    } else {
        body.force += derived.forwardVelocity * (-kCoastDamping * constants.mass);
    }
    if (body.turnRadius == 0.0F) {
        return;
    }
    const Vector3 left = Normalized({.x = -body.facing.y, .y = body.facing.x}, {});
    const float speed = derived.forwardSpeed;
    body.force += left * (constants.mass * speed * speed / body.turnRadius);
    SetYawImpulse(body, constants, YawImpulseRate(body), kYawImpulseMs);
    body.yawRate = speed / body.turnRadius;
}

void AddGroundedForces(BodyState& body, const BodyConstants& constants) {
    const BodyMotion& motion = body.motion;
    const float threshold = AverageOverWheels(motion, &Surface::supportThreshold);
    if (Length(body.supportForce) > constants.gravity * constants.massScale * threshold) {
        body.force += body.supportForce;
    }
    if (body.hover.isHovering && !constants.rules.hoversOnWheels) {
        AddHoverForces(body);
    } else if (body.impulses.rollMs <= 0 && body.impulses.pitchMs <= 0) {
        AddWheelForces(body, constants);
    }
    float contactScale = 1.0F;
    if (body.steering.isSteering || body.spin.isSpinning) {
        const float friction = AverageOverWheels(motion, &Surface::friction);
        body.force += body.derived.velocityDirection *
                      (GravityForce(constants) * friction * body.steering.slip);
        contactScale = 1.0F - body.steering.slip;
    }
    if (body.spin.isSpinning || body.hasWallYaw) {
        body.force +=
            body.derived.lateralVelocity * (-kWallYawDamping * constants.mass * contactScale);
    } else {
        AddGripForces(body, constants, contactScale);
    }
}

void AddSurfaceAndDrag(BodyState& body, const BodyConstants& constants) {
    const bool isHovering = body.hover.isHovering;
    if (!isHovering && !body.isSpunOut) {
        Vector3 push;
        for (const WheelProbe& wheel : body.motion.wheels) {
            push += wheel.surface.force;
        }
        body.force += push * (constants.massScale * kWheelShare);
    }
    float rolling = 0.0F;
    if (!isHovering) {
        for (const WheelProbe& wheel : body.motion.wheels) {
            rolling += wheel.surface.rollingResistance;
        }
    }
    const float drag = (kWheelShare * rolling) + body.dragCoefficient;
    body.force += body.motion.velocity * (-drag * body.derived.speed);
}

void AddExternalForces(BodyState& body) {
    for (const std::optional<Vector3>& force : body.externalForces) {
        if (force) {
            body.force += *force;
        }
    }
}

}

void AccumulateForces(BodyState& body, const BodyConstants& constants) {
    if (body.steering.isSteering &&
        Dot(body.motion.basis.forward, body.derived.velocityDirection) <= 0.0F) {
        StopSteering(body.steering);
    }
    body.yawRate = 0.0F;
    if (IsGrounded(body.motion)) {
        AddGroundedForces(body, constants);
    } else {
        AddAirborneForces(body, constants);
    }
    AddSurfaceAndDrag(body, constants);
    AddExternalForces(body);
}

}
