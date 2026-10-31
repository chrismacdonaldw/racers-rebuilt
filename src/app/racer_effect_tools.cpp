#include "app/racer_effect_tools.hpp"

#include <cstddef>
#include <string_view>

namespace rr::app {
namespace {

constexpr float kMissileTurns = 2.0F;
constexpr float kMissileSpinRate = 0.007F;
constexpr float kLightningStrength = 1.0F;
constexpr Milliseconds kCurse{10'000};
constexpr float kGrapplePull = 180.0F;
constexpr std::size_t kPushSlot = 0;

bool IsPushed(const sim::Racer& racer) {
    return racer.Route() ? racer.Route()->Effects().isPushed
                         : racer.Body().externalForces[kPushSlot].has_value();
}

std::string Toggle(bool isOn, std::string_view start, std::string_view stop) {
    return std::string(isOn ? stop : start);
}

}

std::string RacerEffectLabel(RacerEffect effect, const sim::Racer& racer) {
    const sim::RacerStatus& status = racer.Status();
    switch (effect) {
        case RacerEffect::Spin:
            return "Spin, as a missile";
        case RacerEffect::LaunchHit:
            return "Launch, as lightning";
        case RacerEffect::Hover:
            return Toggle(status.isHalted, "Hover", "Stop hovering");
        case RacerEffect::Curse:
            return Toggle(status.isCursed, "Curse", "Lift the curse");
        case RacerEffect::Push:
            return Toggle(IsPushed(racer), "Push forward", "Stop pushing");
        case RacerEffect::MagnetHold:
            return Toggle(status.isMagnetHeld, "Magnet hold", "Release the magnet");
        case RacerEffect::Ghost:
            return Toggle(status.isGhost, "Ghost", "Stop ghosting");
    }
    return {};
}

void ApplyRacerEffect(RacerEffect effect, sim::Racer& racer) {
    const sim::RacerStatus& status = racer.Status();
    switch (effect) {
        case RacerEffect::Spin:
            racer.StartSpin(kMissileTurns, kMissileSpinRate);
            racer.ReactToHit();
            break;
        case RacerEffect::LaunchHit:
            racer.LaunchHit(kLightningStrength);
            racer.ReactToHit();
            break;
        case RacerEffect::Hover:
            if (status.isHalted) {
                racer.Resume();
            } else {
                racer.Halt();
            }
            break;
        case RacerEffect::Curse:
            if (status.isCursed) {
                racer.RemoveCurse();
            } else {
                racer.AttachCurse(kCurse);
            }
            break;
        case RacerEffect::Push:
            if (IsPushed(racer)) {
                racer.EndExternalForce(kPushSlot);
            } else {
                racer.StartExternalForce(kPushSlot, racer.Body().facing * kGrapplePull);
            }
            break;
        case RacerEffect::MagnetHold:
            if (status.isMagnetHeld) {
                racer.EndMagnetHold();
            } else {
                racer.StartMagnetHold();
            }
            break;
        case RacerEffect::Ghost:
            if (status.isGhost) {
                racer.LeaveGhostMode();
            } else {
                racer.EnterGhostMode();
            }
            break;
    }
}

}
