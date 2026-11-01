#pragma once

#include "assets/event_id.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/track/track_event.hpp"

namespace rr::sim {

inline constexpr assets::EventId kWarpPadEvent = 0;

/// Runs from load and warps each racer that fires its event on the racer path.
struct WarpPad {
    static void Hear(const TrackEvent& event, HazardContext& context);
};

}
