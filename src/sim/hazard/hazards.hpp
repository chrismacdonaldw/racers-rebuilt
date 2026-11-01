#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

#include "assets/event_id.hpp"
#include "assets/hazard_list.hpp"
#include "sim/hazard/breakable.hpp"
#include "sim/hazard/code_puzzle.hpp"
#include "sim/hazard/crane.hpp"
#include "sim/hazard/falling_pillar.hpp"
#include "sim/hazard/ghost.hpp"
#include "sim/hazard/hammer_swish.hpp"
#include "sim/hazard/hazard_box.hpp"
#include "sim/hazard/hazard_cannon.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/hazard/lava_blob.hpp"
#include "sim/hazard/rocket_pad.hpp"
#include "sim/hazard/sphinx.hpp"
#include "sim/hazard/ufo_grabber.hpp"
#include "sim/race/random.hpp"
#include "sim/track/model_switches.hpp"
#include "sim/track/track_entities.hpp"
#include "sim/track/track_event.hpp"
#include "sim/track/track_events.hpp"

namespace rr::sim {

enum class HazardPhase : std::uint8_t { Loaded, Active };

using HazardState =
    std::variant<std::monostate, FallingPillar, Sphinx, HammerSwish, Crane, HazardCannon, Breakable,
                 HazardBox, Ghost, LavaBlob, CodePuzzle, RocketPad, UfoGrabber>;

struct HazardSlot {
    assets::EventId trigger = assets::kNoEvent;
    HazardPhase phase = HazardPhase::Loaded;
    HazardState state;
};

class Hazards {
public:
    Hazards(std::span<const assets::Hazard> hazards, TrackEntities& entities, Random& random);

    [[nodiscard]] static std::size_t MostEventsPerTick(std::span<const assets::Hazard> hazards,
                                                       std::size_t racerCount);

    /// Also hears what hazards fire meanwhile when `heard` is `context.events`, up to a cap.
    void Hear(const TrackEvents& heard, HazardContext& context);

    void Tick(HazardContext& context);

    /// Run after the racers step, with `context.bodies` set.
    void Touch(HazardContext& context);

    [[nodiscard]] std::span<const HazardSlot> Slots() const { return slots_; }
    [[nodiscard]] const ModelSwitches& Models() const { return models_; }
    [[nodiscard]] ModelSwitches& Models() { return models_; }

private:
    std::vector<HazardSlot> slots_;
    ModelSwitches models_;
};

}
