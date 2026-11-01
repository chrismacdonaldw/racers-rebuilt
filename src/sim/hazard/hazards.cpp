#include "sim/hazard/hazards.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kMostOwnEventsPerTick = 2;
constexpr std::size_t kMostEventsPerRacerHit = 2;

HazardSlot Load(const assets::Hazard& hazard, TrackEntities& entities) {
    switch (hazard.kind) {
        case assets::HazardKind::HammerSwish:
            return {.trigger = kHammerSwishTrigger, .state = LoadHammerSwish(entities)};
        case assets::HazardKind::Crane:
            return {.trigger = kCraneTrigger, .state = LoadCrane(entities)};
        case assets::HazardKind::SwingingBox:
            if (const auto* payload = std::get_if<assets::SwingingBox>(&hazard.payload)) {
                return {.trigger = payload->trigger, .state = LoadHazardBox(*payload, entities)};
            }
            return {};
        default:
            return {};
    }
}

}

Hazards::Hazards(std::span<const assets::Hazard> hazards, TrackEntities& entities) {
    slots_.reserve(hazards.size());
    for (const assets::Hazard& hazard : hazards) {
        slots_.push_back(Load(hazard, entities));
    }
}

std::size_t Hazards::MostEventsPerTick(std::size_t hazardCount, std::size_t racerCount) {
    return hazardCount * (kMostOwnEventsPerTick + (racerCount * kMostEventsPerRacerHit));
}

void Hazards::Hear(std::span<const TrackEvent> events) {
    for (const TrackEvent& event : events) {
        if (event.racer) {
            continue;
        }
        for (HazardSlot& slot : slots_) {
            if (slot.trigger != event.id) {
                continue;
            }
            if (event.edge == EventEdge::Start && slot.phase == HazardPhase::Loaded) {
                slot.phase = HazardPhase::Active;
                std::visit(
                    [](auto& hazard) {
                        if constexpr (requires { hazard.Activate(); }) {
                            hazard.Activate();
                        }
                    },
                    slot.state);
            } else if (event.edge == EventEdge::End) {
                slot.phase = HazardPhase::Loaded;
            }
        }
    }
}

void Hazards::Tick(HazardContext& context) {
    for (HazardSlot& slot : slots_) {
        if (slot.phase == HazardPhase::Active) {
            std::visit(
                [&](auto& hazard) {
                    if constexpr (requires { hazard.Tick(context); }) {
                        hazard.Tick(context);
                    }
                },
                slot.state);
        }
    }
}

void Hazards::Touch(HazardContext& context) {
    for (const HazardSlot& slot : slots_) {
        if (slot.phase == HazardPhase::Active) {
            std::visit(
                [&](const auto& hazard) {
                    if constexpr (requires { hazard.Touch(context); }) {
                        hazard.Touch(context);
                    }
                },
                slot.state);
        }
    }
}

}
