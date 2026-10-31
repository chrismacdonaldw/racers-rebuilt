#pragma once

#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "sim/physics/body_state.hpp"

namespace rr::sim {

struct RouteSpin {
    int remainingMs = 0;
    float ratePerMs = 0.0F;
    float angle = 0.0F;
};

struct RouteTilt {
    int remainingMs = 0;
    float ratePerMs = 0.0F;
    float angle = 0.0F;
    float height = 0.0F;
};

/// In units and units per s.
struct RouteHop {
    float velocity = 0.0F;
    float height = 0.0F;
};

struct RouteEffects {
    bool isSpinning = false;
    bool isSpunOut = false;
    bool isHovering = false;
    bool isBoosting = false;
    bool isGhost = false;
    bool isCurseSlowed = false;
    bool isPushed = false;
    RouteSpin spin;
    RouteTilt tilt;
    RouteHop hop;
    HoverLift lift;
};

/// Starts the nose-up tilt unless one is already running.
void StartRouteTilt(RouteTilt& tilt, float ratePerMs, int durationMs);

void UpdateRouteHop(RouteEffects& effects, int dtMs);

/// Banks, tilts, spins and lifts the route's pose. `wheelAnchor` is in body space.
[[nodiscard]] Transform PlaceOnRoute(RouteEffects& effects, const Transform& routePose,
                                     const Vector3& wheelAnchor, int dtMs);

}
