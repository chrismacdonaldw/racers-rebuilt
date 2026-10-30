#include "sim/physics/racer.hpp"

#include <cmath>
#include <utility>

#include "core/assert.hpp"
#include "sim/physics/car_body.hpp"
#include "sim/physics/drive_controller.hpp"
#include "sim/physics/facing.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr float kMinWorldZ = -250.0F;
constexpr float kMaxWorldZ = 340.0F;
constexpr float kMinDriftSpeed = 0.03F;
constexpr Milliseconds kMaxStep{40};

RacerControls SteeringOnly(const RacerControls& controls) {
    return {.isLeftHeld = controls.isLeftHeld,
            .isRightHeld = controls.isRightHeld,
            .steerLeftAxis = controls.steerLeftAxis};
}

Basis HeadingBasis(float headingRadians) {
    return OrthonormalBasis({.x = std::cos(headingRadians), .y = std::sin(headingRadians)},
                            {.z = 1.0F});
}

}

Racer::Racer(const ChassisSpec& chassis, const CarBuild& build, const RacerStart& start)
    : constants_(MakeBodyConstants(chassis, build)), stats_(MakeStatMultipliers(chassis, build)) {
    Place(start);
}

void Racer::Place(const RacerStart& start) {
    body_ = MakeBody(constants_, start.position, HeadingBasis(start.headingRadians));
    SetDrive(body_, constants_, 0.0F, kBaseMaxSpeed * stats_.topSpeed);
    driver_ = {};
    turbo_.reset();
}

void Racer::Tick(Milliseconds elapsed, const RacerControls& controls, const CollisionQuery& world) {
    RR_ASSERT(elapsed > Milliseconds{0} && elapsed <= kMaxStep);
    events_ = {};
    if (route_) {
        if (!startBoost_.State().isHeldAtStart) {
            route_->Tick(elapsed);
            TakeRoutePose(*route_);
        }
        return;
    }
    const auto dtMs = static_cast<int>(elapsed.count());
    if (const std::optional<TurboLevel> boost = startBoost_.Update(controls, dtMs)) {
        StartTurbo(*boost);
    }
    const bool wasDrifting = driver_.State().isDrifting;
    const RacerControls driven =
        startBoost_.State().isHeldAtStart ? SteeringOnly(controls) : controls;
    DriveCommand command = driver_.Update(driven, CanStartDrift(body_), dtMs);
    if (command.isDrifting && body_.derived.forwardSpeed < kMinDriftSpeed) {
        driver_.EndDrift();
        command.isDrifting = false;
        command.isHardDrift = false;
    }
    events_.hasDriftStarted = command.isDrifting && !wasDrifting;
    if (turbo_ && body_.hasWallContact) {
        turbo_->hasTouchedWall = true;
    }
    ApplyDriveCommand(body_, constants_, command, stats_, turbo_.transform(TurboThrust));
    StepBody(body_, constants_, dtMs, world);
    KeepInWorldBounds();
    UpdateTurbo(dtMs);
}

void Racer::HoldAtStart() {
    startBoost_.HoldAtStart();
}

void Racer::Go() {
    if (const std::optional<TurboLevel> boost = startBoost_.Go(); boost && !route_) {
        StartTurbo(*boost);
    }
}

void Racer::FollowRoute(assets::RaceRoute route) {
    TakeRoutePose(route_.emplace(std::move(route)));
}

void Racer::SetRouteSpeed(float speed) {
    if (route_) {
        route_->SetTargetSpeed(speed);
    }
}

void Racer::StartTurbo(TurboLevel level) {
    turbo_ = sim::StartTurbo(body_, constants_, stats_, level);
    events_.turboStarted = level;
}

void Racer::UpdateTurbo(int dtMs) {
    if (!turbo_) {
        return;
    }
    const TurboPhase phase = turbo_->phase;
    if (!AdvanceTurbo(*turbo_, dtMs)) {
        turbo_.reset();
        EndTurbo(body_, constants_, stats_);
    } else if (phase != TurboPhase::Fade && turbo_->phase == TurboPhase::Fade) {
        events_.turboFaded = turbo_->level;
    }
}

Transform Racer::Pose() const {
    return {.position = body_.motion.position, .rotation = ToQuaternion(body_.motion.basis)};
}

void Racer::TakeRoutePose(const AiRacer& route) {
    const Transform pose = route.Pose();
    const Quaternion& rotation = pose.rotation;
    BodyMotion& motion = body_.motion;
    body_.saved = motion;
    motion.position = pose.position;
    motion.basis = {.forward = Rotate(rotation, {.x = 1.0F}),
                    .side = Rotate(rotation, {.y = 1.0F}),
                    .up = Rotate(rotation, {.z = 1.0F})};
    motion.velocity = route.Velocity();
    motion.contactCount = route.ContactCount();
    body_.facing = motion.basis.forward;
    UpdateDerivedMotion(body_);
}

void Racer::KeepInWorldBounds() {
    const float z = body_.motion.position.z;
    if (z >= kMinWorldZ && z <= kMaxWorldZ) {
        return;
    }
    body_.motion = body_.saved;
    body_.motion.velocity = {};
}

}
