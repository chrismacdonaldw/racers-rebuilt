#include "sim/physics/body_effects.hpp"

#include <algorithm>
#include <utility>

#include "core/assert.hpp"
#include "sim/physics/ground_contact.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr float kTwoPi = 6.2831855F;
constexpr int kSpinOutMs = 600;
constexpr float kLaunchForward = 80.0F;
constexpr float kLaunchUp = 200.0F;
constexpr float kHoverLiftTarget = 6.0F;
constexpr float kHoverLiftRate = 3.0F;
constexpr float kHoverPitchRate = -0.003F;
constexpr int kHoverPitchMs = 150;

}

void StartSpin(BodyState& body, const BodyConstants& constants, float turns, float ratePerMs,
               float align) {
    SpinState& spin = body.spin;
    if (spin.isSpinning) {
        return;
    }
    CancelAngularMomentum(body.motion, body.motion.basis.up);
    spin.periodMs = static_cast<int>(kTwoPi / ratePerMs);
    SetYawImpulse(body, constants, ratePerMs,
                  static_cast<int>(static_cast<float>(spin.periodMs) * turns));
    spin.isSpinning = true;
    spin.isFresh = true;
    if (align > 0.0F) {
        spin.stopAtMs =
            static_cast<int>(static_cast<float>(spin.periodMs) * std::min(align, turns));
    }
}

void FinishSpin(BodyState& body) {
    SpinState& spin = body.spin;
    if (spin.stopAtMs == 0) {
        return;
    }
    while (spin.stopAtMs > spin.periodMs) {
        spin.stopAtMs -= spin.periodMs;
    }
    body.impulses.yawMs = spin.stopAtMs;
    spin.stopAtMs = 0;
    spin.periodMs = 0;
}

void EndSpin(BodyState& body) {
    if (!body.spin.isSpinning) {
        return;
    }
    body.impulses.yawMs = 0;
    CancelAngularMomentum(body.motion, body.motion.basis.up);
    body.spin.isSpinning = false;
    body.spin.isFresh = false;
    body.hasWallYaw = false;
}

int CountDownSpin(BodyState& body, int dtMs) {
    SpinState& spin = body.spin;
    int& yawMs = body.impulses.yawMs;
    if (spin.isFresh) {
        spin.isFresh = false;
    } else if (dtMs < yawMs) {
        yawMs -= dtMs;
    } else if (yawMs != 0) {
        dtMs = std::exchange(yawMs, 0);
    } else {
        EndSpin(body);
    }
    return dtMs;
}

void StartSpinOut(BodyState& body) {
    body.isSpunOut = true;
    body.spinOutMs = kSpinOutMs;
}

void EndSpinOut(BodyState& body) {
    body.isSpunOut = false;
    body.spinOutMs = 0;
}

void CountDownSpinOut(BodyState& body, int dtMs) {
    if (!body.isSpunOut) {
        return;
    }
    if (dtMs >= body.spinOutMs) {
        EndSpinOut(body);
    } else {
        body.spinOutMs -= dtMs;
    }
}

void ApplyLaunch(BodyState& body, const BodyConstants& constants, float strength) {
    StartSpinOut(body);
    body.motion.velocity =
        (body.facing * (kLaunchForward * strength) + Vector3{.z = kLaunchUp * strength}) *
        (1.0F / constants.mass);
}

void StartHover(BodyState& body, const BodyConstants& constants) {
    HoverState& hover = body.hover;
    hover.isHovering = true;
    SetPitchImpulse(body, constants, kHoverPitchRate, kHoverPitchMs);
    hover.lift = {.height = 0.0F,
                  .targetHeight = kHoverLiftTarget,
                  .ratePerSecond = kHoverLiftRate,
                  .bank = 0.0F,
                  .targetBank = 0.0F};
    PlaceWheels(body.motion, constants);
    const auto& wheels = body.motion.wheels;
    hover.frontAxle = (wheels[0].position + wheels[1].position) * 0.5F;
    hover.rearAxle = (wheels[2].position + wheels[3].position) * 0.5F;
}

void StopHover(BodyState& body) {
    body.hover.isHovering = false;
    body.hover.lift.height = 0.0F;
    body.hover.lift.bank = 0.0F;
    body.hover.lift.targetBank = 0.0F;
}

void StartExternalForce(BodyState& body, const BodyConstants& constants, std::size_t slot,
                        const Vector3& force) {
    RR_ASSERT(slot < kExternalForceSlots);
    body.externalForces.at(slot) = force * constants.massScale;
}

void EndExternalForce(BodyState& body, std::size_t slot) {
    RR_ASSERT(slot < kExternalForceSlots);
    body.externalForces.at(slot).reset();
}

}
