#include "sim/physics/drive_controller.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "sim/physics/car_body.hpp"
#include "sim/physics/ground_contact.hpp"

namespace rr::sim {
namespace {

constexpr float kTightCurvature = 1.0F / 40.0F;
constexpr float kWideCurvature = 1.0F / 4000.0F;
constexpr float kThrust = 54.0F;
constexpr float kBrakeScale = 2.0F;
constexpr float kCrawlSpeed = 0.04F;
constexpr float kCrawlRadiusScale = 0.2F;
constexpr float kAirborneGripLimit = 100.0F;
constexpr float kGriplessLimit = 4096.0F;
constexpr float kSteerAlignment = 0.9F;
constexpr float kMaxSteerSpeed = 0.155F;
constexpr float kGripSlip = 0.85F;
constexpr float kHardDriftSlip = 0.25F;
constexpr float kGripLagMaximum = 0.7071F;
constexpr float kMinDriftShare = 0.05F;
constexpr float kDriftAlignment = 0.85F;
constexpr float kMinDriftSpeed = 0.05F;

float ThrustFor(float drive, float forwardSpeed) {
    const float thrust = kThrust * drive;
    return drive * forwardSpeed < 0.0F ? thrust * kBrakeScale : thrust;
}

float Alignment(const BodyState& body) {
    return Dot(body.motion.basis.forward, body.derived.velocityDirection);
}

bool CanSteer(const BodyState& body, float radius) {
    const float alignment = Alignment(body);
    return alignment > 0.0F && radius * body.turnRadius >= 0.0F &&
           (body.steering.isSteering || alignment >= kSteerAlignment) && !body.hasWallContact &&
           body.derived.speed <= kMaxSteerSpeed;
}

float AssistGrip(BodyState& body, float radius, float limit) {
    const float magnitude = std::abs(radius);
    if (CanSteer(body, radius) && magnitude > 0.0F && magnitude < limit) {
        StartSteering(body.steering, 2.0F - (magnitude / limit), kGripSlip, kGripLagMaximum);
    } else {
        StopSteering(body.steering);
    }
    return magnitude < limit ? (limit + magnitude) / 2.0F : magnitude;
}

float AssistDrift(BodyState& body, float radius, float limit, bool isHard) {
    const float magnitude = std::abs(radius);
    const float share = std::max(kMinDriftShare, 1.0F - (magnitude / limit));
    const float pi = std::numbers::pi_v<float>;
    if (isHard) {
        StartSteering(body.steering, (2.0F * share) + 1.0F, kHardDriftSlip, pi);
    } else {
        StartSteering(body.steering, share + 1.0F, kGripSlip, pi);
    }
    return magnitude < limit ? magnitude + ((limit - magnitude) / 4.0F) : magnitude;
}

}

float TurnRadiusFor(float steering) {
    if (steering > 0.0F) {
        return 1.0F / (kWideCurvature + ((kTightCurvature - kWideCurvature) * steering));
    }
    if (steering < 0.0F) {
        return 1.0F / (((kTightCurvature - kWideCurvature) * steering) - kWideCurvature);
    }
    return 0.0F;
}

float SteeringFor(float radius) {
    if (radius == 0.0F) {
        return 0.0F;
    }
    const float curvature = 1.0F / std::abs(radius);
    const float steering = (curvature - kWideCurvature) / (kTightCurvature - kWideCurvature);
    return std::copysign(std::max(steering, 0.0F), radius);
}

float GripLimit(const BodyState& body) {
    if (!IsGrounded(body.motion)) {
        return kAirborneGripLimit;
    }
    const float grip = AverageContactGrip(body.motion) * body.gripScale;
    if (grip <= 0.0F) {
        return kGriplessLimit;
    }
    const float speed = body.derived.forwardSpeed;
    return speed * speed * 1.0e6F / (grip * kGravity);
}

bool CanStartDrift(const BodyState& body) {
    return !body.hasWallContact && body.motion.contactCount >= 3 &&
           Alignment(body) > kDriftAlignment && body.derived.forwardSpeed >= kMinDriftSpeed;
}

void ApplyDriveCommand(BodyState& body, const BodyConstants& constants, const DriveCommand& command,
                       const StatMultipliers& stats, std::optional<float> turboThrust) {
    float radius = TurnRadiusFor(command.steering * stats.handling);
    const float thrust = turboThrust.value_or(ThrustFor(command.drive, body.derived.forwardSpeed)) *
                         stats.acceleration;
    SetDrive(body, constants, thrust, body.maxSpeed);
    if (radius == 0.0F) {
        StopSteering(body.steering);
        SetTurnRadius(body, 0.0F);
        return;
    }
    if (body.derived.speed < kCrawlSpeed) {
        radius *= kCrawlRadiusScale;
    }
    if (command.isDrifting && !CanSteer(body, radius)) {
        StopSteering(body.steering);
        SetTurnRadius(body, radius);
        return;
    }
    const float limit = GripLimit(body);
    const float magnitude = command.isDrifting
                                ? AssistDrift(body, radius, limit, command.isHardDrift)
                                : AssistGrip(body, radius, limit);
    SetTurnRadius(body, std::copysign(magnitude, radius));
}

}
