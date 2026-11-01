#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

#include "assets/event_id.hpp"
#include "assets/hazard_list.hpp"
#include "sim/hazard/crane.hpp"
#include "sim/hazard/hammer_swish.hpp"
#include "sim/hazard/hazard_box.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/track/track_entities.hpp"
#include "sim/track/track_event.hpp"

namespace rr::sim {

enum class HazardPhase : std::uint8_t { Loaded, Active };

using HazardState = std::variant<std::monostate, HammerSwish, Crane, HazardBox>;

struct HazardSlot {
    assets::EventId trigger = assets::kNoEvent;
    HazardPhase phase = HazardPhase::Loaded;
    HazardState state;
};

class Hazards {
public:
    Hazards(std::span<const assets::Hazard> hazards, TrackEntities& entities);

    [[nodiscard]] static std::size_t MostEventsPerTick(std::size_t hazardCount,
                                                       std::size_t racerCount);

    /// Starts and ends the hazards that position-path events name by trigger id.
    void Hear(std::span<const TrackEvent> events);

    void Tick(HazardContext& context);

    /// Run after the racers step, with `context.bodies` set.
    void Touch(HazardContext& context);

    [[nodiscard]] std::span<const HazardSlot> Slots() const { return slots_; }

private:
    std::vector<HazardSlot> slots_;
};

}
