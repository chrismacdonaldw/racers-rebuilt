#include "sim/physics/body_effects.hpp"
#include "sim/physics/racer.hpp"

namespace rr::sim {

void Racer::StartSpin(float turns, float ratePerMs, float align) {
    sim::StartSpin(body_, constants_, turns, ratePerMs, align);
}

void Racer::StartSpinOut() {
    sim::StartSpinOut(body_);
}

void Racer::LaunchHit(float strength) {
    ApplyLaunch(body_, constants_, strength);
}

void Racer::Halt() {
    if (status_.isHalted) {
        return;
    }
    status_.isHalted = true;
    EndSpin(body_);
    StartHover(body_, constants_);
}

void Racer::Resume() {
    if (!status_.isHalted) {
        return;
    }
    status_.isHalted = false;
    StopHover(body_);
}

void Racer::AttachCurse(Milliseconds duration) {
    status_.isCursed = true;
    status_.curseMs = static_cast<int>(duration.count());
    ReactToHit();
    if (turbo_) {
        EndTurbo(body_, constants_, stats_);
    }
}

void Racer::RemoveCurse() {
    status_.isCursed = false;
    status_.curseMs = 0;
}

void Racer::StartExternalForce(std::size_t slot, const Vector3& force) {
    sim::StartExternalForce(body_, constants_, slot, force);
}

void Racer::EndExternalForce(std::size_t slot) {
    sim::EndExternalForce(body_, slot);
}

void Racer::StartMagnetHold() {
    status_.isMagnetHeld = true;
    Resume();
}

void Racer::EndMagnetHold() {
    status_.isMagnetHeld = false;
    Resume();
}

void Racer::EnterGhostMode() {
    status_.isGhost = true;
    RemoveCurse();
    EndSpin(body_);
    body_.hasTrackCollision = false;
}

void Racer::LeaveGhostMode() {
    status_.isGhost = false;
    body_.hasTrackCollision = true;
}

void Racer::ReactToHit() {
    ++status_.hitReactions;
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
    return body_.spin.isSpinning;
}

}
