#include "sim/track/trigger_lists.hpp"

#include "core/assert.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

std::vector<assets::TriggerSphere> Placed(std::span<const assets::TriggerSphere> spheres,
                                          bool isMirrored) {
    std::vector<assets::TriggerSphere> placed(spheres.begin(), spheres.end());
    if (isMirrored) {
        for (assets::TriggerSphere& sphere : placed) {
            sphere.centre.y = -sphere.centre.y;
        }
    }
    return placed;
}

bool Accepts(const assets::TriggerSphere& sphere, const TriggerRacer& racer) {
    return !sphere.isForProjectiles && (racer.isPlayer || !sphere.isPlayerOnly) &&
           (!sphere.lap || *sphere.lap == racer.completedLaps) &&
           Touches({.centre = sphere.centre, .radius = sphere.radius}, racer.reach);
}

}

RacerTriggers::RacerTriggers(std::span<const assets::TriggerSphere> spheres, bool isMirrored)
    : spheres_(Placed(spheres, isMirrored)), masks_(spheres_.size()) {}

void RacerTriggers::EndFrame(TrackEventQueue& queue) {
    for (std::size_t i = 0; i < spheres_.size(); ++i) {
        Masks& masks = masks_[i];
        const auto left = static_cast<std::uint8_t>(masks.wasInside & ~masks.inside);
        for (std::uint8_t racer = 0; racer < kMaxTriggerRacers; ++racer) {
            if ((left >> racer & 1U) != 0) {
                queue.Fire({.id = spheres_[i].event, .edge = EventEdge::End, .racer = racer});
            }
        }
        masks.wasInside = masks.inside;
        masks.inside = 0;
    }
}

void RacerTriggers::Test(std::span<const TriggerRacer> racers, TrackEventQueue& queue) {
    RR_ASSERT(racers.size() <= kMaxTriggerRacers);
    for (std::size_t i = 0; i < spheres_.size(); ++i) {
        Masks& masks = masks_[i];
        for (std::size_t racer = 0; racer < racers.size(); ++racer) {
            if (!Accepts(spheres_[i], racers[racer])) {
                continue;
            }
            const auto bit = static_cast<std::uint8_t>(1U << racer);
            if (((masks.wasInside | masks.inside) & bit) == 0) {
                queue.Fire({.id = spheres_[i].event,
                            .edge = EventEdge::Start,
                            .racer = static_cast<std::uint8_t>(racer)});
            }
            masks.inside |= bit;
        }
    }
}

EnvironmentTriggers::EnvironmentTriggers(std::span<const assets::TriggerSphere> spheres,
                                         bool isMirrored)
    : spheres_(Placed(spheres, isMirrored)), states_(spheres_.size()) {}

void EnvironmentTriggers::EndFrame(TrackEventQueue& queue) {
    for (std::size_t i = 0; i < spheres_.size(); ++i) {
        State& state = states_[i];
        if (state.isActive && !state.isTouched) {
            queue.Fire(
                {.id = spheres_[i].event, .edge = EventEdge::End, .position = spheres_[i].centre});
            state.isActive = false;
        }
        state.isTouched = false;
    }
}

void EnvironmentTriggers::Test(std::span<const TriggerRacer> racers, TrackEventQueue& queue) {
    for (std::size_t i = 0; i < spheres_.size(); ++i) {
        State& state = states_[i];
        for (const TriggerRacer& racer : racers) {
            if (!Accepts(spheres_[i], racer)) {
                continue;
            }
            if (!state.isActive) {
                queue.Fire({.id = spheres_[i].event,
                            .edge = EventEdge::Start,
                            .position = spheres_[i].centre});
                state.isActive = true;
            }
            state.isTouched = true;
        }
    }
}

}
