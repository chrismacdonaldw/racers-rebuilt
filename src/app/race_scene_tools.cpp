#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <span>
#include <utility>

#include "app/checkpoint_gates.hpp"
#include "app/powerup_tools.hpp"
#include "app/race_scene.hpp"
#include "app/race_step.hpp"
#include "app/racer_effect_tools.hpp"

namespace rr::app {

overlay::RacerTools RaceScene::OverlayRacerTools() {
    overlay::RacerTools tools{.selected = &chosenRacer_};
    const std::span<const sim::Racer> racers = std::as_const(parts_.session).Racers();
    for (std::size_t i = 0; i < racers.size(); ++i) {
        tools.racers.push_back(i == kPlayerSlot ? std::format("Racer {} (you)", i + 1)
                                                : std::format("Racer {}", i + 1));
    }
    tools.groups.push_back(EffectTools());
    tools.groups.push_back(PowerupTools());
    return tools;
}

overlay::RacerActionGroup RaceScene::EffectTools() {
    overlay::RacerActionGroup group{.name = "Effects"};
    const sim::Racer& racer = std::as_const(parts_.session).Racers()[chosenRacer_];
    for (const RacerEffect effect : kRacerEffects) {
        group.actions.push_back({.name = RacerEffectLabel(effect, racer), .run = [this, effect] {
                                     chosenTools_.emplace_back(
                                         [racer = chosenRacer_, effect](sim::RaceSession& race) {
                                             ApplyRacerEffect(effect, race.Racers()[racer]);
                                         });
                                 }});
    }
    return group;
}

overlay::RacerActionGroup RaceScene::PowerupTools() {
    overlay::RacerActionGroup group{.name = "Give power-up"};
    for (const assets::BrickColour colour : kBrickColours) {
        for (std::uint8_t level = 0; level <= sim::kMaxPowerupLevel; ++level) {
            group.actions.push_back(
                {.name = PowerupLabel(colour, level), .run = [this, colour, level] {
                     chosenTools_.emplace_back(
                         [racer = chosenRacer_, colour, level](sim::RaceSession& race) {
                             race.GivePowerup(racer, colour, level);
                         });
                 }});
        }
    }
    return group;
}

void RaceScene::ApplyChosenTools() {
    for (const std::function<void(sim::RaceSession&)>& tool : chosenTools_) {
        tool(parts_.session);
    }
    chosenTools_.clear();
}

void RaceScene::DrawDebugLines() {
    debugLines_.clear();
    if (showsCheckpoints_) {
        DrawGates(parts_.raceTrack->checkpoints,
                  parts_.session.Progress().OfRacer(kPlayerSlot).laps, debugLines_);
    }
    if (showsCollision_) {
        debugLines_.insert(debugLines_.end(), collisionLines_.begin(), collisionLines_.end());
    }
    if (showsTriggers_) {
        triggerRings_.Light(parts_.session.Triggers());
        debugLines_.insert(debugLines_.end(), triggerRings_.Lines().begin(),
                           triggerRings_.Lines().end());
    }
}

}
