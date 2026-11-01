#include "sim/physics/body_effects.hpp"
#include "sim/physics/facing.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr float kWarpLaunchImpulse = 700.0F;

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

void Racer::AttachCurse(Milliseconds duration) {
    status_.isCursed = true;
    status_.curseMs = static_cast<int>(duration.count());
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
        route_->Hold();
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

void Racer::RollCurse(Milliseconds elapsed, Random& random) {
    UpdateCurseJitter(status_.jitter, static_cast<int>(elapsed.count()), status_.isCursed, random);
}

void Racer::UpdateTimers(Milliseconds elapsed) {
    if (!route_ && body_.isSpunOut) {
        EndSpinOut(body_);
    }
    const auto elapsedMs = static_cast<int>(elapsed.count());
    if (elapsedMs <= status_.curseMs) {
        status_.curseMs -= elapsedMs;
    } else if (status_.isCursed) {
        RemoveCurse();
    }
}

bool Racer::IsSpinning() const {
    return route_ ? route_->Effects().isSpinning : body_.spin.isSpinning;
}

}
