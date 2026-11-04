#include <cstddef>
#include <cstdint>
#include <optional>

#include "sim/ai/route_driver.hpp"
#include "sim/race/player_cheats.hpp"
#include "sim/race/race_session.hpp"

namespace rr::sim {
namespace {

bool AreBricksAudible(const RaceClock& clock) {
    return clock.Phase() != RacePhase::Intro && clock.Phase() != RacePhase::Countdown;
}

}

bool RaceSession::WantsPowerup(std::size_t racer, Milliseconds elapsed,
                               const RacerControls& controls) {
    if (isPlayer_[racer]) {
        return controls.isUsePressed;
    }
    if (racers_[racer].Boost().isHeldAtStart) {
        return false;
    }
    const std::optional<RouteDriver>& driver = drivers_[racer];
    const bool isLiningUp = driver.has_value() && driver->Craft().aims;
    return aiPowerups_[racer].Tick(elapsed,
                                   {.racers = racers_,
                                    .racer = racer,
                                    .inventory = &inventories_[racer],
                                    .isCrowded = powerups_.IsCrowded(),
                                    .hasFinished = progress_.OfRacer(racer).finishPlace.has_value(),
                                    .isLiningUp = isLiningUp},
                                   random_);
}

void RaceSession::StartBoosts() {
    PowerupContext effects = EffectContext();
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        if (!isPlayer_[i] && aiPowerups_[i].RollsStartBoost(random_)) {
            powerups_.Fire({.racer = static_cast<std::uint8_t>(i),
                            .colour = assets::BrickColour::Green,
                            .level = 0},
                           effects);
        }
    }
}

void RaceSession::GivePowerup(std::size_t racer, assets::BrickColour colour, std::uint8_t level) {
    PowerupInventory& inventory = inventories_.at(racer);
    bricks_.Spend(inventory);
    inventory.Grant(colour, level);
}

PowerupContext RaceSession::EffectContext() {
    return {.racers = racers_,
            .world = course_.world,
            .random = &random_,
            .checkpoints = course_.checkpoints,
            .progress = &progress_,
            .reaches = reaches_,
            .powerups = &powerups_,
            .bricks = &bricks_,
            .inventories = inventories_,
            .explosions = &explosions_,
            .spikes = &spikes_,
            .weapons = &weapons_,
            .surfaces = &surfaces_,
            .trackEvents = &triggers_,
            .hazardAims = hazards_.Aims()};
}

void RaceSession::TickBricks(Milliseconds elapsed) {
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        pickupReaches_[i] = PickupReach(reaches_[i], CheatsOf(i));
    }
    bricks_.Tick(elapsed, pickupReaches_, inventories_);
    sounds_.HearBricks(bricks_.Events(), AreBricksAudible(clock_));
    for (const BrickEvent& event : bricks_.Events()) {
        if (event.kind == BrickEventKind::WhiteCollected && event.whiteCount == kMaxPowerupLevel) {
            racers_[event.racer].React(VoiceReaction::Positive);
        }
        const bool tookColour =
            event.kind == BrickEventKind::Collected || event.kind == BrickEventKind::Swapped;
        if (modifiers_.hasChaosBricks && tookColour) {
            RollChaosBrick(inventories_[event.racer]);
        }
    }
}

void RaceSession::RollChaosBrick(PowerupInventory& inventory) {
    bricks_.Spend(inventory);
    const auto colour = static_cast<assets::BrickColour>(
        random_.Below(static_cast<std::uint32_t>(kBrickColourCount)));
    inventory.Grant(colour, random_.Below(kMaxPowerupLevel + 1));
}

void RaceSession::KeepWhitesFull() {
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        if (CheatsOf(i).keepsFullWhite) {
            inventories_[i].FillWhites();
        }
    }
}

PlayerCheats RaceSession::CheatsOf(std::size_t racer) const {
    return isPlayer_[racer] ? playerCheats_ : PlayerCheats{};
}

void RaceSession::UsePowerup(std::size_t racer) {
    PowerupContext effects = EffectContext();
    if (isPlayer_[racer]) {
        effects.targets = targets_.Left();
    }
    const std::optional<assets::BrickColour> colour = inventories_[racer].Colour();
    const std::size_t whites = inventories_[racer].WhiteCount();
    if ((colour == assets::BrickColour::Yellow || colour == assets::BrickColour::Blue) &&
        !cheats_.Has(Cheat::RedOnly)) {
        racers_[racer].React(VoiceReaction::Positive);
    }
    const PlayerCheats cheats = CheatsOf(racer);
    if (cheats.hasFreeWarp && colour == assets::BrickColour::Green) {
        GivePowerup(racer, *colour, kMaxPowerupLevel);
    }
    powerups_.Use(racer, inventories_[racer], bricks_, effects);
    if (!colour) {
        Taunt(racer);
    } else if (cheats.hasBottomlessBrick) {
        GivePowerup(racer, *colour, static_cast<std::uint8_t>(whites));
    }
}

bool RaceSession::UseButtonCheats(std::size_t racer, const RacerControls& controls) {
    const PlayerCheats cheats = CheatsOf(racer);
    if (cheats.hasBoostButton && controls.isBoostPressed) {
        PowerupContext effects = EffectContext();
        powerups_.Fire({.racer = static_cast<std::uint8_t>(racer),
                        .colour = assets::BrickColour::Green,
                        .level = 1},
                       effects);
    }
    return cheats.hasSkipAhead && controls.isSkipPressed && SkipAhead(racer);
}

bool RaceSession::SkipAhead(std::size_t racer) {
    Racer& car = racers_[racer];
    if (car.Status().isWarping || car.Status().isGhost) {
        return false;
    }
    const std::size_t gate = progress_.NextGate(*course_.checkpoints, racer).value_or(0);
    const float speed = car.Body().derived.speed;
    car.Place(RespawnPoint(*course_.checkpoints, gate, course_.grid[racer], *course_.world));
    car.Push(car.Body().motion.basis.forward, speed * car.Constants().mass);
    return true;
}

void RaceSession::Taunt(std::size_t racer) {
    triggers_.FireStartAndEnd(kTauntEvent, positions_[racer]);
    sounds_.HearTaunt(racer);
}

}
