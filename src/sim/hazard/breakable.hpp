#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "assets/event_id.hpp"
#include "assets/hazard_list.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/track/model_switches.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::sim {

inline constexpr assets::EventId kPropBreakEvent = 9;

struct Breakable {
    static constexpr bool kIgnoresEnd = true;

    std::string surface;
    std::vector<std::size_t> entities;
    std::vector<std::size_t> models;
    bool isBroken = false;

    void Activate(HazardContext& context);

    /// Ends the hazard once every prop has played its part, leaving them hidden and passable.
    [[nodiscard]] bool Tick(HazardContext& context);
};

/// Freezes the props until the trigger.
[[nodiscard]] Breakable LoadBreakable(const assets::BreakableProp& payload, TrackEntities& entities,
                                      ModelSwitches& models);

}
