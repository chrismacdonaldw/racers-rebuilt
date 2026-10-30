#pragma once

#include <array>
#include <cmath>
#include <numbers>

#include "core/vector3.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/physics/surface.hpp"

namespace rr::sim {

inline constexpr float kGravity = 39.0F;

struct Basis {
    Vector3 forward{.x = 1.0F};
    Vector3 side{.y = 1.0F};
    Vector3 up{.z = 1.0F};
};

struct WheelProbe {
    Vector3 position;
    bool hasHit = false;
    Vector3 hitPoint;
    Vector3 hitNormal;
    float hitHeight = 0.0F;
    bool isInContact = false;
    Vector3 contactForce;
    Surface hitSurface;
    Surface surface;
};

struct BodyMotion {
    Vector3 position;
    Basis basis;
    Vector3 velocity;
    Vector3 angularMomentum;
    std::array<WheelProbe, kWheelCount> wheels{};
    int contactCount = 0;
    Vector3 supportNormal{.z = 1.0F};
};

struct DerivedMotion {
    float speed = 0.0F;
    Vector3 velocityDirection{.x = 1.0F};
    float forwardSpeed = 0.0F;
    Vector3 forwardVelocity;
    Vector3 lateralVelocity;
    bool isAtRest = false;
};

struct SteeringState {
    bool isSteering = false;
    float gain = 1.0F;
    float slip = 0.0F;
    float alignmentMinimum = -1.0F;
    float lagMaximum = std::numbers::pi_v<float>;
};

struct ImpulseTimers {
    int yawMs = 0;
    int pitchMs = 0;
    int rollMs = 0;
};

/// Distances are world units and times are ms, so velocities are units per ms.
struct BodyState {
    BodyMotion motion;
    BodyMotion saved;
    Vector3 angularVelocity;
    Vector3 force;
    Vector3 torque;
    Vector3 facing{.x = 1.0F};
    DerivedMotion derived;
    float turnRadius = 0.0F;
    float yawRate = 0.0F;
    float thrust = 0.0F;
    float maxSpeed = 110.0F;
    float dragCoefficient = 0.0F;
    float gripScale = 1.0F;
    SteeringState steering;
    ImpulseTimers impulses;
    bool hasWallYaw = false;
    bool hasWallContact = false;
    int airborneMs = 0;
    Vector3 supportForce;
};

[[nodiscard]] constexpr bool IsGrounded(const BodyMotion& motion) {
    return motion.contactCount > 0;
}

inline void StartSteering(SteeringState& steering, float gain, float slip, float lagMaximum) {
    steering = {.isSteering = true,
                .gain = gain,
                .slip = slip,
                .alignmentMinimum = std::cos(lagMaximum),
                .lagMaximum = lagMaximum};
}

inline void StopSteering(SteeringState& steering) {
    steering.isSteering = false;
    steering.alignmentMinimum = -1.0F;
    steering.lagMaximum = std::numbers::pi_v<float>;
}

}
