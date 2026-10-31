#include "sim/ai/route_driver.hpp"

#include <algorithm>
#include <cmath>

#include "core/quaternion.hpp"
#include "sim/physics/drive_controller.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kCursorStep{12};
constexpr Milliseconds kReferenceLead{100};
constexpr Milliseconds kCurveSpan{250};
constexpr int kMaxCursorSteps = 256;
constexpr float kCorrectionDistance = 30.0F;
constexpr float kMinCurvature = 1.0F / 4096.0F;
constexpr float kDriftReleaseLock = 0.8F;

Vector3 Flat(const Vector3& v) {
    return {.x = v.x, .y = v.y};
}

Vector3 FlatForward(const Quaternion& rotation) {
    return Normalized(Flat(Rotate(rotation, {.x = 1.0F})), {.x = 1.0F});
}

bool IsPast(const Transform& pose, const Vector3& position) {
    return Dot(Flat(position - pose.position), FlatForward(pose.rotation)) > 0.0F;
}

float CurvatureToward(const Vector3& from, const Vector3& forward, const Vector3& to) {
    const Vector3 chord = Flat(to - from);
    const float lengthSquared = Dot(chord, chord);
    if (lengthSquared <= 0.0F) {
        return 0.0F;
    }
    const Vector3 left{.x = -forward.y, .y = forward.x};
    return 2.0F * Dot(chord, left) / lengthSquared;
}

float GripTurnRadius(float radius, float limit) {
    return radius >= limit ? radius : (2.0F * radius) - limit;
}

float DriftTurnRadius(float radius, float limit) {
    return radius >= limit ? radius : ((4.0F * radius) - limit) / 3.0F;
}

float LockFor(float radius, float handling) {
    return radius <= 0.0F ? 2.0F : SteeringFor(radius) / handling;
}

}

RouteDriver::RouteDriver(const assets::RaceRoute& route)
    : nearest_(route), reference_(route), ahead_(route) {}

RacerControls RouteDriver::Drive(const Racer& racer) {
    const BodyMotion& motion = racer.Body().motion;
    Follow(motion.position);
    const Transform reference = reference_.Pose();
    const Vector3 tangent = FlatForward(reference.rotation);
    const Vector3 target = Flat(reference.position) + (tangent * kCorrectionDistance);
    const Vector3 forward = Normalized(Flat(motion.basis.forward), {.x = 1.0F});
    const float curvature = CurvatureToward(reference.position, tangent, ahead_.Pose().position) +
                            CurvatureToward(motion.position, forward, target);
    if (std::abs(curvature) < kMinCurvature) {
        return {.isThrottleHeld = true};
    }
    const float radius = 1.0F / std::abs(curvature);
    const float limit = GripLimit(racer.Body());
    const float handling = racer.Stats().handling;
    const float gripLock = LockFor(GripTurnRadius(radius, limit), handling);
    const bool isDrifting = gripLock > (racer.Driver().isDrifting ? kDriftReleaseLock : 1.0F);
    const float lock = isDrifting ? LockFor(DriftTurnRadius(radius, limit), handling) : gripLock;
    return {.isThrottleHeld = true,
            .isDriftHeld = isDrifting,
            .steerLeftAxis = std::copysign(std::min(lock, 1.0F), curvature)};
}

void RouteDriver::Follow(const Vector3& position) {
    for (int step = 0; step < kMaxCursorSteps && IsPast(nearest_.Pose(), position); ++step) {
        nearest_.Advance(kCursorStep);
        referenceLead_ -= kCursorStep;
        aheadLead_ -= kCursorStep;
    }
    for (int step = 0; step < kMaxCursorSteps && referenceLead_ < kReferenceLead; ++step) {
        reference_.Advance(kCursorStep);
        referenceLead_ += kCursorStep;
    }
    for (int step = 0; step < kMaxCursorSteps && aheadLead_ < kReferenceLead + kCurveSpan; ++step) {
        ahead_.Advance(kCursorStep);
        aheadLead_ += kCursorStep;
    }
}

}
