#include "sim/hazard/hazards.hpp"

#include <concepts>
#include <type_traits>

namespace rr::sim {
namespace {

constexpr std::size_t kMostOwnEventsPerTick = 2;
constexpr std::size_t kMostCannonEventsPerTick = 10;
constexpr std::size_t kMostEventsPerRacerHit = 2;
constexpr std::size_t kMostHeardFiresPerHazard = 8;

template <typename Payload, typename Make>
HazardSlot WithPayload(const assets::Hazard& hazard, Make make) {
    if (const auto* payload = std::get_if<Payload>(&hazard.payload)) {
        return make(*payload);
    }
    return {};
}

HazardSlot Load(const assets::Hazard& hazard, TrackEntities& entities, ModelSwitches& models,
                Random& random) {
    switch (hazard.kind) {
        case assets::HazardKind::FallingPillar:
            return {.trigger = kFallingPillarTrigger, .state = LoadFallingPillar(entities)};
        case assets::HazardKind::Sphinx:
            return {.trigger = kSphinxTrigger, .state = LoadSphinx(entities, models)};
        case assets::HazardKind::Cannon:
            return WithPayload<assets::CannonShot>(hazard, [](const auto& payload) {
                return HazardSlot{.state = LoadCannon(payload)};
            });
        case assets::HazardKind::BreakableProp:
            return WithPayload<assets::BreakableProp>(hazard, [&](const auto& payload) {
                return HazardSlot{.trigger = payload.trigger,
                                  .state = LoadBreakable(payload, entities, models)};
            });
        case assets::HazardKind::ShipCannons:
            return WithPayload<assets::ShipCannons>(hazard, [](const auto& payload) {
                return HazardSlot{.state = LoadShipCannon(payload)};
            });
        case assets::HazardKind::HammerSwish:
            return {.trigger = kHammerSwishTrigger, .state = LoadHammerSwish(entities)};
        case assets::HazardKind::Crane:
            return {.trigger = kCraneTrigger, .state = LoadCrane(entities)};
        case assets::HazardKind::SwingingBox:
            return WithPayload<assets::SwingingBox>(hazard, [&](const auto& payload) {
                return HazardSlot{.trigger = payload.trigger,
                                  .state = LoadHazardBox(payload, entities)};
            });
        case assets::HazardKind::Ghost:
            return {.trigger = kGhostTrigger, .state = LoadGhost(entities)};
        case assets::HazardKind::LavaBlob:
            return {.trigger = kLavaBlobTrigger, .state = LoadLavaBlob(entities)};
        case assets::HazardKind::CodePuzzle:
            return {.phase = HazardPhase::Active, .state = LoadCodePuzzle(random)};
        case assets::HazardKind::RocketPad:
            return {.trigger = kRocketPadTrigger, .state = LoadRocketPad(entities, models)};
        case assets::HazardKind::UfoGrabber:
            return WithPayload<assets::UfoGrabber>(hazard, [&](const auto& payload) {
                return HazardSlot{.trigger = payload.trigger,
                                  .state = LoadUfoGrabber(payload, entities)};
            });
        default:
            return {};
    }
}

template <typename Run>
void RunActive(std::vector<HazardSlot>& slots, Run run) {
    for (HazardSlot& slot : slots) {
        if (slot.phase == HazardPhase::Active) {
            std::visit(run, slot.state);
        }
    }
}

bool StartsOn(const HazardState& state, const TrackEvent& event) {
    return std::visit(
        [&](const auto& hazard) {
            if constexpr (requires { hazard.StartsOn(event); }) {
                return hazard.StartsOn(event);
            } else {
                return false;
            }
        },
        state);
}

bool IgnoresEnd(const HazardState& state) {
    return std::visit(
        [](const auto& hazard) {
            return requires { std::remove_cvref_t<decltype(hazard)>::kIgnoresEnd; };
        },
        state);
}

void Switch(HazardSlot& slot, const TrackEvent& event, HazardContext& context) {
    if (event.edge == EventEdge::Start && slot.phase == HazardPhase::Loaded) {
        slot.phase = HazardPhase::Active;
        std::visit(
            [&](auto& hazard) {
                if constexpr (requires { hazard.Activate(context); }) {
                    hazard.Activate(context);
                } else if constexpr (requires { hazard.Activate(); }) {
                    hazard.Activate();
                }
            },
            slot.state);
    } else if (event.edge == EventEdge::End && slot.phase == HazardPhase::Active &&
               !IgnoresEnd(slot.state)) {
        slot.phase = HazardPhase::Loaded;
        std::visit(
            [&](auto& hazard) {
                if constexpr (requires { hazard.Deactivate(context); }) {
                    hazard.Deactivate(context);
                }
            },
            slot.state);
    }
}

}

Hazards::Hazards(std::span<const assets::Hazard> hazards, TrackEntities& entities, Random& random) {
    slots_.reserve(hazards.size());
    for (const assets::Hazard& hazard : hazards) {
        slots_.push_back(Load(hazard, entities, models_, random));
    }
}

std::size_t Hazards::MostEventsPerTick(std::span<const assets::Hazard> hazards,
                                       std::size_t racerCount) {
    std::size_t count = 0;
    for (const assets::Hazard& hazard : hazards) {
        switch (hazard.kind) {
            case assets::HazardKind::CodePuzzle:
                count += CodePuzzle::MostEventsPerTick(racerCount);
                break;
            case assets::HazardKind::Cannon:
            case assets::HazardKind::ShipCannons:
                count += kMostCannonEventsPerTick;
                break;
            default:
                count += kMostOwnEventsPerTick + (racerCount * kMostEventsPerRacerHit);
        }
    }
    return count;
}

void Hazards::Hear(const TrackEvents& heard, HazardContext& context) {
    const std::size_t limit = heard.Events().size() + (slots_.size() * kMostHeardFiresPerHazard);
    for (std::size_t i = 0; i < heard.Events().size() && i < limit; ++i) {
        const TrackEvent event = heard.Events()[i];
        for (HazardSlot& slot : slots_) {
            if (!event.racer && (slot.trigger == event.id || StartsOn(slot.state, event))) {
                Switch(slot, event, context);
            }
        }
        RunActive(slots_, [&](auto& hazard) {
            if constexpr (requires { hazard.Hear(event, context); }) {
                hazard.Hear(event, context);
            }
        });
    }
}

void Hazards::Tick(HazardContext& context) {
    for (HazardSlot& slot : slots_) {
        if (slot.phase != HazardPhase::Active) {
            continue;
        }
        std::visit(
            [&](auto& hazard) {
                if constexpr (requires {
                                  { hazard.Tick(context) } -> std::same_as<bool>;
                              }) {
                    if (!hazard.Tick(context)) {
                        slot.phase = HazardPhase::Loaded;
                    }
                } else if constexpr (requires { hazard.Tick(context); }) {
                    hazard.Tick(context);
                }
            },
            slot.state);
    }
}

void Hazards::Touch(HazardContext& context) {
    RunActive(slots_, [&](auto& hazard) {
        if constexpr (requires { hazard.Touch(context); }) {
            hazard.Touch(context);
        }
    });
}

}
