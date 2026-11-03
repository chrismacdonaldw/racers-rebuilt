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
constexpr Milliseconds kStallTimeout{3000};
constexpr float kBrakeWindowStartMs = 400.0F;
constexpr float kBrakeWindowEndMs = 650.0F;
constexpr float kBrakeMargin = 1.1F;
constexpr Milliseconds kBackUpFrom{1000};
constexpr Milliseconds kBackUpUntil{1800};
constexpr float kBlockAhead = 25.0F;
constexpr float kBlockWidth = 5.0F;
constexpr float kPassOffset = 6.0F;
constexpr float kPassWidthShare = 0.8F;

Vector3 Flat(const Vector3& v) {
    return {.x = v.x, .y = v.y};
}

Vector3 LeftOf(const Vector3& forward) {
    return {.x = -forward.y, .y = forward.x};
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
    return 2.0F * Dot(chord, LeftOf(forward)) / lengthSquared;
}

float GripTurnRadius(float radius, float limit) {
    return radius >= limit ? radius : (2.0F * radius) - limit;
}

float DriftTurnRadius(float radius, float limit) {
    return radius >= limit ? radius : ((4.0F * radius) - limit) / 3.0F;
}

float SpeedBetween(const Vector3& from, const Vector3& to, float routeMs) {
    return Length(Flat(to - from)) / routeMs;
}

float LockFor(float radius, float handling) {
    return radius <= 0.0F ? 2.0F : SteeringFor(radius) / handling;
}

}

RouteDriver::RouteDriver(const assets::RaceRoute& route, std::optional<float> pace)
    : nearest_(route), reference_(route), ahead_(route), pace_(pace) {}

RacerControls RouteDriver::Drive(const Racer& racer, std::span<const Vector3> racers,
                                 std::optional<std::size_t> self, std::optional<Vector3> aim) {
    const BodyMotion& motion = racer.Body().motion;
    Follow(motion.position);
    if (stalled_ >= kBackUpFrom && stalled_ < kBackUpUntil) {
        return {.isBrakeHeld = true};
    }
    const Pedal pedal = PedalFor(motion);
    const bool isThrottleHeld = pedal == Pedal::Throttle;
    const bool isBrakeHeld = pedal == Pedal::Brake;
    const Transform reference = reference_.Pose();
    const Vector3 tangent = FlatForward(reference.rotation);
    const float offset = aim ? AimOffset(*aim) : PassOffset(motion, racers, self);
    const Vector3 target =
        Flat(reference.position) + (tangent * kCorrectionDistance) + (LeftOf(tangent) * offset);
    const Vector3 forward = Normalized(Flat(motion.basis.forward), {.x = 1.0F});
    const float curvature = CurvatureToward(reference.position, tangent, ahead_.Pose().position) +
                            CurvatureToward(motion.position, forward, target);
    if (std::abs(curvature) < kMinCurvature) {
        return {.isThrottleHeld = isThrottleHeld, .isBrakeHeld = isBrakeHeld};
    }
    const float radius = 1.0F / std::abs(curvature);
    const float limit = GripLimit(racer.Body());
    const float handling = racer.Stats().handling;
    const float gripLock = LockFor(GripTurnRadius(radius, limit), handling);
    const bool isDrifting = gripLock > (racer.Driver().isDrifting ? kDriftReleaseLock : 1.0F);
    const float lock = isDrifting ? LockFor(DriftTurnRadius(radius, limit), handling) : gripLock;
    return {.isThrottleHeld = isThrottleHeld,
            .isBrakeHeld = isBrakeHeld,
            .isDriftHeld = isDrifting && !isBrakeHeld,
            .steerLeftAxis = std::copysign(std::min(lock, 1.0F), curvature)};
}

RouteDriver::Pedal RouteDriver::PedalFor(const BodyMotion& motion) const {
    const auto leadMs = static_cast<float>((aheadLead_ - referenceLead_).count());
    if (!pace_ || leadMs <= 0.0F) {
        return Pedal::Throttle;
    }
    const Vector3 ahead = ahead_.Pose().position;
    const float routeSpeed = SpeedBetween(reference_.Pose().position, ahead, leadMs);
    const float brakeSpeed = SpeedBetween(ahead_.PoseAhead(kBrakeWindowStartMs).position,
                                          ahead_.PoseAhead(kBrakeWindowEndMs).position,
                                          kBrakeWindowEndMs - kBrakeWindowStartMs);
    const float target = *pace_ * std::min(routeSpeed, brakeSpeed);
    const float speed = Dot(motion.velocity, motion.basis.forward);
    if (speed < target) {
        return Pedal::Throttle;
    }
    return speed > target * kBrakeMargin ? Pedal::Brake : Pedal::Coast;
}

float RouteDriver::PassOffset(const BodyMotion& motion, std::span<const Vector3> racers,
                              std::optional<std::size_t> self) const {
    const Vector3 forward = Normalized(Flat(motion.basis.forward), {.x = 1.0F});
    const Vector3 left = LeftOf(forward);
    for (std::size_t i = 0; i < racers.size(); ++i) {
        if (i == self) {
            continue;
        }
        const Vector3 gap = Flat(racers[i] - motion.position);
        const float ahead = Dot(gap, forward);
        const float side = Dot(gap, left);
        if (ahead > 0.0F && ahead < kBlockAhead && std::abs(side) < kBlockWidth) {
            return side > 0.0F ? -std::min(kPassOffset, reference_.WidthRight() * kPassWidthShare)
                               : std::min(kPassOffset, reference_.WidthLeft() * kPassWidthShare);
        }
    }
    return 0.0F;
}

float RouteDriver::AimOffset(const Vector3& aim) const {
    const Transform reference = reference_.Pose();
    const float side = Dot(Flat(aim - reference.position), LeftOf(FlatForward(reference.rotation)));
    return std::clamp(side, -reference_.WidthRight() * kPassWidthShare,
                      reference_.WidthLeft() * kPassWidthShare);
}

bool RouteDriver::IsStalled(Milliseconds elapsed) {
    stalled_ = hasHeadway_ ? Milliseconds{0} : stalled_ + elapsed;
    if (stalled_ < kStallTimeout) {
        return false;
    }
    stalled_ = Milliseconds{0};
    return true;
}

RacerStart RouteDriver::Rejoin() const {
    const Transform pose = ahead_.Pose();
    const Vector3 forward = FlatForward(pose.rotation);
    return {.position = pose.position, .headingRadians = std::atan2(forward.y, forward.x)};
}

void RouteDriver::Follow(const Vector3& position) {
    hasHeadway_ = IsPast(nearest_.Pose(), position);
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
