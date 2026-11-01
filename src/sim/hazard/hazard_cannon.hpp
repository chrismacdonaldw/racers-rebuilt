#pragma once

#include <optional>
#include <vector>

#include "assets/event_id.hpp"
#include "assets/hazard_list.hpp"
#include "core/vector3.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/powerup/ballistic_flight.hpp"
#include "sim/track/track_event.hpp"

namespace rr::sim {

inline constexpr assets::EventId kCannonFiredEvent = 6;
inline constexpr assets::EventId kCannonLandedEvent = 7;

struct HazardCannon {
    std::vector<assets::AimPoint> launches;
    std::vector<assets::AimPoint> targets;
    assets::EventId trigger = assets::kNoEvent;
    std::optional<Vector3> triggerCentre;
    float triggerRadius = 0.0F;
    bool picksAtRandom = false;
    assets::AimPoint target;
    BallisticFlight flight;
    bool isFlying = false;

    [[nodiscard]] bool StartsOn(const TrackEvent& event) const;
    void Activate(HazardContext& context);

    /// Stays active for good after a ball that runs out without landing, as the original does.
    [[nodiscard]] bool Tick(HazardContext& context);
};

[[nodiscard]] HazardCannon LoadCannon(const assets::CannonShot& payload);

/// Draws a launch point and a target per shot, then fires the target's event where it aimed.
[[nodiscard]] HazardCannon LoadShipCannon(const assets::ShipCannons& payload);

}
