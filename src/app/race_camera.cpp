#include "app/race_camera.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>

#include "app/track_space.hpp"
#include "sim/physics/racer.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace rr::app {
namespace {

struct Profile {
    float pitchDegrees;
    float heightDegrees;
    float followDistance;
    float positionLag;
    float rotationLag;
};

constexpr std::array kProfiles{
    Profile{.pitchDegrees = 5.0F,
            .heightDegrees = 35.0F,
            .followDistance = 20.0F,
            .positionLag = 0.1F,
            .rotationLag = 0.25F},
    Profile{.pitchDegrees = 8.0F,
            .heightDegrees = 25.0F,
            .followDistance = 30.0F,
            .positionLag = 0.1F,
            .rotationLag = 0.25F},
    Profile{.pitchDegrees = 8.0F,
            .heightDegrees = 45.0F,
            .followDistance = 10.0F,
            .positionLag = 0.05F,
            .rotationLag = 0.25F},
    Profile{.pitchDegrees = 0.0F,
            .heightDegrees = 0.0F,
            .followDistance = 0.0F,
            .positionLag = 0.1F,
            .rotationLag = 0.25F},
    Profile{.pitchDegrees = 20.0F,
            .heightDegrees = 15.0F,
            .followDistance = 32.0F,
            .positionLag = 0.18F,
            .rotationLag = 0.35F},
    Profile{.pitchDegrees = 8.0F,
            .heightDegrees = 20.0F,
            .followDistance = 22.0F,
            .positionLag = 0.1F,
            .rotationLag = 0.25F},
    Profile{.pitchDegrees = 7.0F,
            .heightDegrees = 15.0F,
            .followDistance = 45.0F,
            .positionLag = 0.1F,
            .rotationLag = 0.25F},
    Profile{.pitchDegrees = 4.0F,
            .heightDegrees = 30.0F,
            .followDistance = 15.0F,
            .positionLag = 0.05F,
            .rotationLag = 0.25F},
};

constexpr std::size_t kCycledViews = 4;
constexpr std::size_t kSplitChaseViews = 3;
constexpr std::size_t kSplitProfileOffset = 5;
constexpr float kLagTimeScale = 250.0F;
constexpr float kMaxElapsedMs = 100.0F;
constexpr float kTargetRate = 0.09F;
constexpr float kTurnLeadRate = 0.0044444446F;
constexpr float kViewAngleRate = 0.016666668F;
constexpr float kTurnLookAhead = 80.0F;
constexpr float kMaxTurnLead = 0.3F;
constexpr float kViewAngleDeadband = 0.014F;
constexpr float kZoomInRecoveryPerMs = 0.004F;
constexpr float kZoomOutRecoveryPerMs = 0.002F;
constexpr float kMinDistance = 12.0F;
constexpr float kFinishSwingMs = 2000.0F;
constexpr float kFinishTurnPerMs = 0.0015707964F;
constexpr float kCockpitHeight = 10.0F;
constexpr float kSplitCockpitHeight = 6.0F;
constexpr Vector3 kForward{.x = 1.0F};
constexpr Vector3 kUp{.z = 1.0F};
constexpr float kPi = std::numbers::pi_v<float>;

float Lag(float amount) {
    return (1.0F - amount) / (amount * kLagTimeScale);
}

float Ease(float rate, float elapsedMs) {
    return 1.0F / ((rate * elapsedMs) + 1.0F);
}

float Radians(float degrees) {
    return degrees * kPi / 180.0F;
}

Vector3 Flat(const Vector3& v, const Vector3& fallback) {
    return Normalized({.x = v.x, .y = v.y}, fallback);
}

Vector3 Turned(const Vector3& v, float radians) {
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    return {.x = (cosine * v.x) - (sine * v.y), .y = (cosine * v.y) + (sine * v.x)};
}

Vector3 Led(const Vector3& v, float radians) {
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    const float x = (cosine * v.x) - (sine * v.y);
    return {.x = x, .y = (cosine * v.y) + (sine * x)};
}

float Acos(float value) {
    return std::acos(std::clamp(value, -1.0F, 1.0F));
}

float Heading(const Vector3& v) {
    if (std::abs(v.y) < std::abs(v.x)) {
        return v.x > 0.0F ? (kPi / 2.0F) - Acos(v.y) : Acos(v.y) + (kPi / 2.0F);
    }
    return v.y > 0.0F ? Acos(v.x) : -Acos(v.x);
}

float Wrapped(float radians) {
    while (radians < -kPi) {
        radians += 2.0F * kPi;
    }
    while (radians > kPi) {
        radians -= 2.0F * kPi;
    }
    return radians;
}

float TurboZoom(sim::TurboLevel level) {
    switch (level) {
        case sim::TurboLevel::Weak:
            return -0.2F;
        case sim::TurboLevel::Strong:
            return -0.4F;
        case sim::TurboLevel::Hover:
            return -0.6F;
    }
    return 1.0F;
}

glm::vec3 ToGlm(const Vector3& v) {
    return {v.x, v.y, v.z};
}

Quaternion FromGlm(const glm::quat& q) {
    return {.w = q.w, .x = q.x, .y = q.y, .z = q.z};
}

Quaternion Facing(const Vector3& forward, const Vector3& up) {
    const glm::vec3 x = glm::normalize(ToGlm(forward));
    const glm::vec3 z = glm::normalize(ToGlm(up) - (x * glm::dot(ToGlm(up), x)));
    return FromGlm(glm::quat_cast(glm::mat3(x, glm::cross(z, x), z)));
}

Transform ViewToEngine(const Vector3& eye, const Vector3& forward, const Vector3& up) {
    return {.position = ToEngine(eye),
            .rotation = FromGlm(glm::quatLookAt(glm::normalize(ToGlm(ToEngine(forward))),
                                                glm::normalize(ToGlm(ToEngine(up)))))};
}

}

CameraSubject SubjectOf(const sim::Racer& racer) {
    return {.pose = racer.Pose(),
            .forwardSpeed = racer.Body().derived.forwardSpeed,
            .turnRadius = racer.Body().turnRadius,
            .isSpinning = racer.IsSpinning(),
            .turbo =
                racer.ActiveTurbo().transform([](const sim::Turbo& turbo) { return turbo.level; })};
}

RaceCamera::RaceCamera(const CameraSubject& subject)
    : target_(subject.pose.position),
      viewDirection_(Rotate(subject.pose.rotation, kForward)),
      hadTurbo_(subject.turbo.has_value()) {
    SetView(CameraView::Standard, false);
    Follow(subject, 0.0F);
}

void RaceCamera::SetView(CameraView view, bool isSplitScreen) {
    auto index = static_cast<std::size_t>(view);
    if (isSplitScreen && index < kSplitChaseViews) {
        index += kSplitProfileOffset;
    }
    const Profile& profile = kProfiles.at(index);
    const Mode mode = ModeOf(view);
    if (mode != mode_) {
        mode_ = mode;
        finishMs_ = 0.0F;
    }
    view_ = view;
    isSplitScreen_ = isSplitScreen;
    pitchSine_ = std::sin(Radians(profile.pitchDegrees));
    pitchCosine_ = std::cos(Radians(profile.pitchDegrees));
    heightSine_ = std::sin(Radians(profile.heightDegrees));
    followDistance_ = profile.followDistance;
    positionLag_ = Lag(profile.positionLag);
    rotationLag_ = Lag(profile.rotationLag);
    isLookingBack_ = false;
}

RaceCamera::Mode RaceCamera::ModeOf(CameraView view) {
    switch (view) {
        case CameraView::Cockpit:
            return Mode::Cockpit;
        case CameraView::Finish:
            return Mode::Finish;
        case CameraView::Standard:
        case CameraView::Far:
        case CameraView::Close:
            break;
    }
    return Mode::Chase;
}

void RaceCamera::CycleView() {
    SetView(static_cast<CameraView>((static_cast<std::size_t>(view_) + 1) % kCycledViews),
            isSplitScreen_);
    Cut();
}

void RaceCamera::Cut() {
    isCut_ = true;
}

void RaceCamera::SetLookBack(bool isLookingBack) {
    isLookingBack_ = isLookingBack;
}

void RaceCamera::Follow(const CameraSubject& subject, float elapsedMs) {
    if (elapsedMs <= 0.0F && !isCut_) {
        return;
    }
    const bool isCut = std::exchange(isCut_, false);
    const float elapsed = std::clamp(elapsedMs, 0.0F, kMaxElapsedMs);
    const Shot raw =
        mode_ == Mode::Cockpit ? FollowCockpit(subject) : FollowChase(subject, elapsed, isCut);
    Smooth(raw, elapsed, isCut);
}

RaceCamera::Shot RaceCamera::FollowChase(const CameraSubject& subject, float elapsedMs,
                                         bool isCut) {
    viewDirection_ = Flat(viewDirection_, kForward);
    const Vector3 heading = Flat(Rotate(subject.pose.rotation, kForward), viewDirection_);
    Vector3 desired = heading;
    if (isCut) {
        turnLead_ = 0.0F;
        target_ = subject.pose.position;
    } else {
        desired = SteerView(subject, heading, elapsedMs);
        target_ = Lerp(subject.pose.position, target_, Ease(kTargetRate, elapsedMs));
    }
    float distance = FollowDistance(subject, elapsedMs);
    viewDirection_ = Flat(desired, kForward);
    Vector3 offset = viewDirection_ * pitchCosine_;
    if (mode_ == Mode::Finish) {
        finishMs_ = std::min(finishMs_ + elapsedMs, kFinishSwingMs);
        distance = followDistance_;
        offset = finishMs_ < kFinishSwingMs
                     ? Turned(viewDirection_, finishMs_ * kFinishTurnPerMs) * pitchCosine_
                     : -viewDirection_ * pitchCosine_;
    }
    offset.z = -pitchSine_;
    const Vector3 behind = isCut ? offset : Rotate(shot_.rotation, kForward);
    const Vector3 position{.x = target_.x - (behind.x * distance),
                           .y = target_.y - (behind.y * distance),
                           .z = target_.z - ((behind.z - heightSine_) * distance)};
    return {.position = position, .rotation = Facing(offset, kUp)};
}

Vector3 RaceCamera::SteerView(const CameraSubject& subject, const Vector3& heading,
                              float elapsedMs) {
    if (subject.isSpinning) {
        return viewDirection_;
    }
    const float turn = subject.turnRadius == 0.0F
                           ? 0.0F
                           : std::clamp(subject.forwardSpeed * kTurnLookAhead / subject.turnRadius,
                                        -kMaxTurnLead, kMaxTurnLead);
    turnLead_ = turn + (Ease(kTurnLeadRate, elapsedMs) * (turnLead_ - turn));
    const Vector3 led = Led(heading, turnLead_);
    const float wanted = Heading(led);
    const float delta = Heading(viewDirection_) - wanted;
    if (std::abs(delta) <= kViewAngleDeadband) {
        return led;
    }
    const float angle = (Wrapped(delta) * Ease(kViewAngleRate, elapsedMs)) + wanted;
    return {.x = std::cos(angle), .y = std::sin(angle)};
}

float RaceCamera::FollowDistance(const CameraSubject& subject, float elapsedMs) {
    if (!hadTurbo_ && subject.turbo) {
        zoom_ = TurboZoom(*subject.turbo);
    }
    hadTurbo_ = subject.turbo.has_value();
    if (zoom_ >= 1.0F) {
        return followDistance_;
    }
    zoom_ += elapsedMs * (zoom_ < 0.0F ? kZoomInRecoveryPerMs : kZoomOutRecoveryPerMs);
    if (zoom_ > 1.0F) {
        zoom_ = 1.0F;
        return followDistance_;
    }
    return std::max(std::abs(zoom_) * followDistance_, kMinDistance);
}

RaceCamera::Shot RaceCamera::FollowCockpit(const CameraSubject& subject) {
    target_ = subject.pose.position;
    const float height = isSplitScreen_ ? kSplitCockpitHeight : kCockpitHeight;
    return {.position = target_ + (kUp * height),
            .rotation = Facing(Rotate(subject.pose.rotation, kForward),
                               Rotate(subject.pose.rotation, kUp))};
}

void RaceCamera::Smooth(const Shot& raw, float elapsedMs, bool isCut) {
    if (isCut) {
        shot_ = raw;
        return;
    }
    shot_ = {.position = Lerp(raw.position, shot_.position, Ease(positionLag_, elapsedMs)),
             .rotation = Nlerp(raw.rotation, shot_.rotation, Ease(rotationLag_, elapsedMs))};
}

Vector3 RaceCamera::ViewDirection() const {
    if (mode_ == Mode::Finish) {
        return viewDirection_;
    }
    return Flat(Rotate(shot_.rotation, kForward), viewDirection_);
}

Transform RaceCamera::Pose() const {
    const Vector3 forward = Rotate(shot_.rotation, kForward);
    const Vector3 up = Rotate(shot_.rotation, kUp);
    if (!isLookingBack_ || mode_ == Mode::Finish) {
        return ViewToEngine(shot_.position, forward, up);
    }
    if (mode_ == Mode::Cockpit) {
        return ViewToEngine(shot_.position, -forward, up);
    }
    const Vector3 eye{.x = (2.0F * target_.x) - shot_.position.x,
                      .y = (2.0F * target_.y) - shot_.position.y,
                      .z = shot_.position.z};
    return ViewToEngine(eye, {.x = -forward.x, .y = -forward.y, .z = forward.z},
                        {.x = -up.x, .y = -up.y, .z = up.z});
}

}
