#include <algorithm>
#include <numbers>
#include <optional>
#include <utility>

#include "sim/physics/body_effects.hpp"
#include "sim/physics/facing.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr float kWarpLaunchImpulse = 700.0F;
constexpr std::size_t kGrabForce = 1;
constexpr float kGrabbedRouteSpeed = -0.4F;
constexpr std::uint32_t kAbsorbSounds = 3;
constexpr int kShieldCooldownMs = 500;
constexpr int kShoveMs = 750;
constexpr float kShoveGain = 2.5F;
constexpr float kShoveSlip = 0.9F;

int CountDown(int remainingMs, int elapsedMs) {
    return std::max(0, remainingMs - elapsedMs);
}

}

void Racer::StartSpin(float turns, float ratePerMs, float align) {
    if (route_) {
        route_->StartSpin(turns, ratePerMs);
    } else {
        sim::StartSpin(body_, constants_, turns, ratePerMs, align);
    }
}

void Racer::StartSpinOut() {
    if (route_) {
        route_->StartSpinOut();
    } else {
        sim::StartSpinOut(body_);
    }
}

void Racer::LaunchHit(float strength) {
    if (route_) {
        route_->StartSpinOut();
    } else {
        ApplyLaunch(body_, constants_, strength);
    }
}

void Racer::KickPitch(float ratePerMs, Milliseconds duration) {
    const auto durationMs = static_cast<int>(duration.count());
    if (route_) {
        route_->Tilt(ratePerMs, durationMs);
    } else {
        SetPitchImpulse(body_, constants_, ratePerMs, durationMs);
    }
}

void Racer::Push(const Vector3& direction, float impulse) {
    if (route_) {
        route_->Bump(direction, impulse);
    } else {
        body_.motion.velocity += direction * (impulse / constants_.mass);
    }
}

void Racer::Stop() {
    if (!route_) {
        body_.motion.velocity = {};
    }
}

void Racer::Halt() {
    if (status_.isHalted) {
        return;
    }
    status_.isHalted = true;
    if (route_) {
        route_->EndSpin();
        route_->StartHover();
    } else {
        EndSpin(body_);
        StartHover(body_, constants_);
    }
}

void Racer::Resume() {
    if (!status_.isHalted) {
        return;
    }
    status_.isHalted = false;
    if (route_) {
        route_->StopHover();
    } else {
        StopHover(body_);
    }
}

void Racer::AttachCurse(Milliseconds duration, const CurseSource& source) {
    status_.isCursed = true;
    status_.curseMs = static_cast<int>(duration.count());
    status_.curseSource = source;
    ReactToHit();
    if (route_) {
        route_->StartCurseSlow();
    } else if (turbo_) {
        EndTurbo(body_, constants_, stats_);
    }
}

void Racer::RemoveCurse() {
    status_.isCursed = false;
    status_.curseMs = 0;
    if (route_) {
        route_->EndCurseSlow();
    }
}

void Racer::StartExternalForce(std::size_t slot, const Vector3& force) {
    if (route_) {
        route_->StartPush(force);
    } else {
        sim::StartExternalForce(body_, constants_, slot, force);
    }
}

void Racer::EndExternalForce(std::size_t slot) {
    if (route_) {
        route_->EndPush();
    } else {
        sim::EndExternalForce(body_, slot);
    }
}

void Racer::StartMagnetHold() {
    status_.isMagnetHeld = true;
    if (route_) {
        route_->HoldAt(0.0F);
    }
    Resume();
}

void Racer::EndMagnetHold() {
    status_.isMagnetHeld = false;
    if (route_) {
        route_->Release();
    }
    Resume();
}

void Racer::StartGrab(const Vector3& pull) {
    if (route_) {
        route_->HoldAt(kGrabbedRouteSpeed);
    } else {
        sim::StartExternalForce(body_, constants_, kGrabForce, pull);
    }
}

void Racer::EndGrab() {
    if (route_) {
        route_->Release();
    } else {
        sim::EndExternalForce(body_, kGrabForce);
    }
}

void Racer::EnterGhostMode() {
    status_.isGhost = true;
    RemoveCurse();
    if (route_) {
        route_->EndSpin();
        route_->StartGhost();
    } else {
        EndSpin(body_);
        body_.hasTrackCollision = false;
    }
}

void Racer::LeaveGhostMode() {
    status_.isGhost = false;
    if (route_) {
        route_->EndGhost();
    } else {
        body_.hasTrackCollision = true;
    }
}

void Racer::React(VoiceReaction reaction) {
    if (!events_.reaction) {
        events_.reaction = reaction;
    }
}

void Racer::ReactToHit() {
    ++status_.hitReactions;
}

bool Racer::BeginWarp() {
    if (status_.isGhost || status_.isWarping) {
        return false;
    }
    if (status_.isMagnetHeld) {
        EndMagnetHold();
    }
    status_.isWarping = true;
    return true;
}

void Racer::CancelWarp() {
    status_.isWarping = false;
}

void Racer::EnterWarp() {
    status_.isWarping = false;
    EnterGhostMode();
    if (!route_) {
        for (std::size_t slot = 0; slot < kExternalForceSlots; ++slot) {
            sim::EndExternalForce(body_, slot);
        }
        status_.shoveForce.reset();
        status_.shoveMs = 0;
    }
    events_.hasWarpEntered = true;
}

void Racer::MoveWarp(const Vector3& position) {
    if (!route_) {
        body_.motion.position = position;
    }
}

void Racer::LandWarp(const Vector3& position, const Vector3& direction) {
    if (!route_) {
        BodyMotion& motion = body_.motion;
        motion.position = position;
        motion.basis = OrthonormalBasis(direction, {.z = 1.0F});
        motion.velocity = direction * (kWarpLaunchImpulse / constants_.mass);
        body_.facing = motion.basis.forward;
        UpdateDerivedMotion(body_);
    }
    LeaveGhostMode();
    events_.hasWarpLanded = true;
}

void Racer::RaiseShield(std::uint8_t level) {
    status_.shield.level = level;
    ++status_.shield.raised;
    RemoveCurse();
}

void Racer::LowerShield() {
    status_.shield.level.reset();
}

void Racer::AbsorbShieldHit(Random& random) {
    if (status_.shield.cooldownMs > 0) {
        return;
    }
    status_.shield.absorbSound = static_cast<std::uint8_t>(random.Below(kAbsorbSounds));
    ++status_.shield.absorbs;
    status_.shield.cooldownMs = kShieldCooldownMs;
}

void Racer::Shove(const Vector3& force) {
    if (status_.shoveMs > 0) {
        return;
    }
    const auto freeForce = std::ranges::find_if(
        body_.externalForces, [](const std::optional<Vector3>& slot) { return !slot; });
    if (freeForce == body_.externalForces.end()) {
        return;
    }
    if (!route_) {
        StartSteering(body_.steering, kShoveGain, kShoveSlip, std::numbers::pi_v<float>);
    }
    status_.shoveForce = static_cast<std::size_t>(freeForce - body_.externalForces.begin());
    status_.shoveMs = kShoveMs;
    StartExternalForce(*status_.shoveForce, force);
}

void Racer::RollCurse(Milliseconds elapsed, Random& random) {
    UpdateCurseJitter(status_.jitter, static_cast<int>(elapsed.count()), status_.isCursed, random);
}

void Racer::UpdateTimers(Milliseconds elapsed) {
    if (!route_ && body_.isSpunOut) {
        EndSpinOut(body_);
    }
    const auto elapsedMs = static_cast<int>(elapsed.count());
    status_.shield.cooldownMs = CountDown(status_.shield.cooldownMs, elapsedMs);
    if (status_.shoveForce) {
        status_.shoveMs = CountDown(status_.shoveMs, elapsedMs);
        if (status_.shoveMs == 0) {
            EndExternalForce(*std::exchange(status_.shoveForce, std::nullopt));
        }
    }
    if (elapsedMs <= status_.curseMs) {
        status_.curseMs -= elapsedMs;
    } else if (status_.isCursed) {
        RemoveCurse();
    }
    UpdateContactTimers(elapsedMs);
}

bool Racer::IsSpinning() const {
    return route_ ? route_->Effects().isSpinning : body_.spin.isSpinning;
}

bool Racer::IsSpunOut() const {
    return route_ ? route_->Effects().isSpunOut : body_.isSpunOut;
}

bool Racer::IsStopped() const {
    return route_ ? route_->PlaybackSpeed() == 0.0F : body_.derived.forwardSpeed == 0.0F;
}

}
