#include "sim/physics/turbo.hpp"

#include <array>
#include <cstddef>

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
constexpr std::array kBoostMs{1000, 1500, 5000};
constexpr int kFadeMs = 700;
constexpr int kEarlyFadeBelowMs = 4500;

void SetMaxSpeed(BodyState& body, const BodyConstants& constants, float maxSpeed) {
    SetDrive(body, constants, body.thrust, maxSpeed);
}

}

Turbo StartTurbo(BodyState& body, const BodyConstants& constants, const StatMultipliers& stats,
                 TurboLevel level) {
    SetPitchImpulse(body, constants, kNoseLiftRatePerMs,
                    level == TurboLevel::Weak ? kWeakNoseLiftMs : kStrongNoseLiftMs);
    body.facing = body.motion.basis.forward;
    UpdateDerivedMotion(body);
    body.gripScale = kTurboGripScale;
    SetMaxSpeed(body, constants, kTurboMaxSpeed * stats.topSpeed);
    return {.level = level, .phaseMs = kTurboIgnitionMs};
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
            turbo.phaseMs = kBoostMs.at(static_cast<std::size_t>(turbo.level)) * turbo.boostScale;
            return true;
        case TurboPhase::Boost:
            FadeTurbo(turbo);
            return true;
        case TurboPhase::Fade:
            return false;
    }
    return false;
}

void FadeTurbo(Turbo& turbo) {
    turbo.phase = TurboPhase::Fade;
    turbo.phaseMs = kFadeMs;
}

bool CanFadeEarly(const Turbo& turbo) {
    return turbo.level == TurboLevel::Hover && turbo.phase == TurboPhase::Boost &&
           turbo.phaseMs < kEarlyFadeBelowMs;
}

float TurboThrust(const Turbo& turbo) {
    return turbo.hasTouchedWall ? kWallTurboThrust : kTurboThrust;
}

}
