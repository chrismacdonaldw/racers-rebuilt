#include "sim/physics/turbo.hpp"

#include "sim/physics/car_body.hpp"
#include "sim/physics/facing.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr float kTurboMaxSpeed = 176.0F;
constexpr float kTurboGripScale = 1.5F;
constexpr float kTurboThrust = 432.0F;
constexpr float kWallTurboThrust = 216.0F;
constexpr float kNoseLiftRatePerMs = -0.0025F;
constexpr int kWeakNoseLiftMs = 125;
constexpr int kStrongNoseLiftMs = 250;
constexpr int kIgnitionMs = 400;
constexpr int kWeakBoostMs = 1000;
constexpr int kStrongBoostMs = 1500;
constexpr int kFadeMs = 700;

void SetMaxSpeed(BodyState& body, const BodyConstants& constants, float maxSpeed) {
    SetDrive(body, constants, body.thrust, maxSpeed);
}

}

Turbo StartTurbo(BodyState& body, const BodyConstants& constants, const StatMultipliers& stats,
                 TurboLevel level) {
    const bool isWeak = level == TurboLevel::Weak;
    SetPitchImpulse(body, constants, kNoseLiftRatePerMs,
                    isWeak ? kWeakNoseLiftMs : kStrongNoseLiftMs);
    body.facing = body.motion.basis.forward;
    UpdateDerivedMotion(body);
    body.gripScale = kTurboGripScale;
    SetMaxSpeed(body, constants, kTurboMaxSpeed * stats.topSpeed);
    return {.level = level, .phaseMs = kIgnitionMs};
}

void EndTurbo(BodyState& body, const BodyConstants& constants, const StatMultipliers& stats) {
    body.gripScale = 1.0F;
    SetMaxSpeed(body, constants, kBaseMaxSpeed * stats.topSpeed);
}

bool AdvanceTurbo(Turbo& turbo, int dtMs) {
    if (turbo.phaseMs > dtMs) {
        turbo.phaseMs -= dtMs;
        return true;
    }
    switch (turbo.phase) {
        case TurboPhase::Ignition:
            turbo.phase = TurboPhase::Boost;
            turbo.phaseMs = turbo.level == TurboLevel::Weak ? kWeakBoostMs : kStrongBoostMs;
            return true;
        case TurboPhase::Boost:
            turbo.phase = TurboPhase::Fade;
            turbo.phaseMs = kFadeMs;
            return true;
        case TurboPhase::Fade:
            return false;
    }
    return false;
}

float TurboThrust(const Turbo& turbo) {
    return turbo.hasTouchedWall ? kWallTurboThrust : kTurboThrust;
}

}
