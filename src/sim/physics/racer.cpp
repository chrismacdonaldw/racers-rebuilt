#include "sim/physics/racer.hpp"

#include <cmath>
#include <optional>
#include <utility>

#include "core/assert.hpp"
#include "core/quaternion.hpp"
#include "sim/physics/body_effects.hpp"
#include "sim/physics/car_body.hpp"
#include "sim/physics/drive_controller.hpp"
#include "sim/physics/facing.hpp"
#include "sim/physics/rigid_body.hpp"
#include "sim/physics/wall_sweep.hpp"

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

Racer::Racer(const ChassisSpec& chassis, const CarBuild& build, const RacerStart& start,
             const VersionRules& rules)
    : constants_(MakeBodyConstants(chassis, build, rules)),
      stats_(MakeStatMultipliers(chassis, build)) {
    Place(start);
}

void Racer::Place(const RacerStart& start) {
    body_ = MakeBody(constants_, start.position, HeadingBasis(start.headingRadians));
    SetDrive(body_, constants_, 0.0F, kBaseMaxSpeed * stats_.topSpeed);
    driver_ = {};
    turbo_.reset();
    const RacerStatus kept = std::exchange(status_, {});
    status_.hitReactions = kept.hitReactions;
    status_.suddenStops = kept.suddenStops;
}

void Racer::Tick(Milliseconds elapsed, const RacerControls& controls, const CollisionQuery& world) {
    RR_ASSERT(elapsed > Milliseconds{0} && elapsed <= kMaxStep);
    events_ = {};
    if (route_) {
        if (!startBoost_.State().isHeldAtStart) {
            TickRoute(*route_, elapsed, world);
        }
        return;
    }
    const auto dtMs = static_cast<int>(elapsed.count());
    if (const std::optional<TurboLevel> boost = startBoost_.Update(controls, dtMs)) {
        StartTurbo(*boost);
    }
    if (controls.isBrakeHeld && !wasBrakeHeld_) {
        FinishSpin(body_);
    }
    wasBrakeHeld_ = controls.isBrakeHeld;
    const bool wasDrifting = driver_.State().isDrifting;
    const RacerControls driven =
        startBoost_.State().isHeldAtStart ? SteeringOnly(controls) : controls;
    DriveCommand command =
        AffectedDrive(driver_.Update(driven, CanStartDrift(body_), constants_.rules, dtMs));
    if (command.isDrifting && body_.derived.forwardSpeed < kMinDriftSpeed) {
        driver_.EndDrift();
        command.isDrifting = false;
        command.isHardDrift = false;
    }
    events_.hasDriftStarted = command.isDrifting && !wasDrifting;
    if (turbo_ && body_.hasWallContact) {
        turbo_->hasTouchedWall = true;
    }
    const SteeringState shoveSteering = body_.steering;
    ApplyDriveCommand(body_, constants_, command, stats_, turbo_.transform(TurboThrust));
    if (status_.shoveForce) {
        body_.steering = shoveSteering;
    }
    StepBody(body_, constants_, dtMs, world, &events_.touches);
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
    TakeRoutePose(route_.emplace(std::move(route), constants_.wheelAnchor));
}

void Racer::SetRouteSpeed(float speed) {
    if (route_ && !status_.isMagnetHeld) {
        route_->SetBaseSpeed(speed);
    }
}

void Racer::StartTurbo(TurboLevel level) {
    if (turbo_) {
        Resume();
    }
    ++turboCount_;
    events_.turboStarted = level;
    if (route_) {
        route_->StartBoost();
        turbo_ = Turbo{.level = level, .phaseMs = kTurboIgnitionMs};
    } else {
        turbo_ = sim::StartTurbo(body_, constants_, stats_, level);
        if (status_.isCursed) {
            EndTurbo(body_, constants_, stats_);
        }
    }
    if (level == TurboLevel::Hover) {
        Halt();
    }
}

void Racer::CancelTurbo() {
    if (turbo_) {
        FinishTurbo();
    }
}

DriveCommand Racer::AffectedDrive(DriveCommand command) {
    if (status_.isMagnetHeld) {
        driver_.EndDrift();
        const float drive = body_.derived.forwardSpeed > 0.0F ? -1.0F : 0.0F;
        command = {.steering = command.steering,
                   .drive = drive,
                   .isDrifting = false,
                   .isHardDrift = false};
    }
    if (!status_.isCursed) {
        return command;
    }
    if (status_.isMagnetHeld) {
        command.steering = -command.steering;
        return command;
    }
    return Cursed(command, status_.jitter);
}

void Racer::UpdateTurbo(int dtMs) {
    if (!turbo_) {
        return;
    }
    const TurboPhase phase = turbo_->phase;
    if (!AdvanceTurbo(*turbo_, dtMs)) {
        FinishTurbo();
        return;
    }
    if (HasHoverStalled()) {
        FadeTurbo(*turbo_);
    }
    if (phase != TurboPhase::Fade && turbo_->phase == TurboPhase::Fade) {
        events_.turboFaded = turbo_->level;
    }
}

void Racer::FinishTurbo() {
    turbo_.reset();
    if (route_) {
        route_->EndBoost();
    } else {
        EndTurbo(body_, constants_, stats_);
    }
    Resume();
}

bool Racer::HasHoverStalled() const {
    return !route_ && status_.isHalted && body_.derived.isAtRest && CanFadeEarly(*turbo_);
}

float Racer::DriftRoll() const {
    return sim::DriftRoll(body_, driver_.State().isDrifting, constants_.rules);
}

Vector3 Racer::CentreOfMass() const {
    return CentreOfMassWorld(body_.motion, constants_);
}

Transform Racer::Pose() const {
    const Basis& basis = body_.motion.basis;
    const float bank = body_.hover.lift.bank;
    if (route_ || bank == 0.0F) {
        return {.position = body_.motion.position, .rotation = ToQuaternion(basis)};
    }
    const Vector3 up = Rotate(FromAxisAngle(basis.forward, bank), basis.up);
    return {.position = body_.motion.position,
            .rotation = ToQuaternion(OrthonormalBasis(basis.forward, up))};
}

void Racer::TickRoute(AiRacer& route, Milliseconds elapsed, const CollisionQuery& world) {
    const auto dtMs = static_cast<int>(elapsed.count());
    route.Save();
    route.Tick(elapsed);
    TakeRoutePose(route);
    if (const std::optional<WallHit> hit =
            SweepBodyPoints(body_, constants_, world, &events_.touches)) {
        route.Restore();
        route.Tick(Milliseconds{RetryStepMs(dtMs, hit->fraction)});
        TakeRoutePose(route);
        route.BackOff();
    }
    UpdateTurbo(dtMs);
}

void Racer::TakeRoutePose(const AiRacer& route) {
    const Transform pose = route.Pose();
    BodyMotion& motion = body_.motion;
    body_.saved = motion;
    motion.position = pose.position;
    motion.basis = ToBasis(pose.rotation);
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
