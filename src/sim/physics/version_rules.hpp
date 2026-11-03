#pragma once

#include "assets/game_version.hpp"

namespace rr::sim {

struct SteeringRates {
    float axis = 2.5F;
    float key = 1.25F;
    float drift = 5.0F;
    float hardDrift = 16.0F;
    float centring = 8.25F;
};

/// Where the 1999 and 2001 releases drive differently. The defaults are 1999's.
struct VersionRules {
    SteeringRates steeringPerSecond;
    bool squaresAxis = false;
    bool snapsAxisReversal = false;
    bool placesWheelsByShadow = false;
    float facingRatePerMs = 0.0025F;
    bool separatesFloorHits = false;
    bool unclampsHoverBank = false;
};

[[nodiscard]] const VersionRules& RulesFor(assets::GameVersion version);

}
