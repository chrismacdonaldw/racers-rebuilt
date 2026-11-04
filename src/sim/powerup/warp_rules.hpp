#pragma once

#include "sim/physics/racer.hpp"

namespace rr::sim {

struct WarpRules {
    bool isFair = false;
    bool catchesUp = false;
    bool aimsLanding = false;
    bool endsWhenTakenBack = false;

    friend bool operator==(const WarpRules&, const WarpRules&) = default;
};

/// `routeSpeed` is a computer racer's route playback speed while it warps.
struct WarpPace {
    float walkPerMs = 0.6F;
    float routeSpeed = kWarpRouteSpeed;
    WarpLaunch launch = WarpLaunch::Impulse;

    friend bool operator==(const WarpPace&, const WarpPace&) = default;
};

/// Catching up leaves a race leader a third of what the warp gains over driving.
[[nodiscard]] WarpPace PaceFor(const WarpRules& rules, const WarpPace& original, bool isLeading);

}
