#include "sim/physics/version_rules.hpp"

namespace rr::sim {
namespace {

constexpr VersionRules kRules1999;

constexpr VersionRules kRules2001{
    .steeringPerSecond =
        {.axis = 4.15F, .key = 1.25F, .drift = 11.0F, .hardDrift = 16.0F, .centring = 25.25F},
    .squaresAxis = true,
    .snapsAxisReversal = true,
    .placesWheelsByShadow = true,
    .facingRatePerMs = 3.75F / 1024.0F,
    .separatesFloorHits = true,
    .unclampsHoverBank = true,
    .hoversOnWheels = true,
    .firstCircuitPace = -0.03F,
    .racersStepFirst = true,
    .allowsInputLag = true,
    .keepsOneLapZone = true,
};

}

const VersionRules& RulesFor(assets::GameVersion version) {
    return version == assets::GameVersion::Release2001 ? kRules2001 : kRules1999;
}

}
