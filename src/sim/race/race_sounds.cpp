#include "sim/race/race_sounds.hpp"

#include <algorithm>
#include <array>
#include <optional>

#include "core/assert.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kMostSoundsPerRacer = 10;
constexpr std::size_t kMostRaceWideSounds = 2;
constexpr std::array kLaterBeeps{Milliseconds{1000}, Milliseconds{2000}};
constexpr Milliseconds kWallCooldown{250};

RaceSound BrickSound(BrickEventKind kind) {
    switch (kind) {
        case BrickEventKind::Respawned:
            return RaceSound::BrickRespawn;
        case BrickEventKind::Collected:
            return RaceSound::BrickCollect;
        case BrickEventKind::Swapped:
            return RaceSound::BrickSwap;
        case BrickEventKind::WhiteCollected:
            return RaceSound::WhiteBrickCollect;
    }
    return RaceSound::BrickRespawn;
}

std::optional<RaceSound> WeaponSound(const WeaponEvent& event) {
    const bool isFired = event.kind == WeaponEventKind::Fired;
    if (event.weapon == PowerupKind::Cannonball) {
        return isFired ? RaceSound::CannonFire : RaceSound::CannonHit;
    }
    if (event.weapon == PowerupKind::Missile) {
        return isFired ? RaceSound::MissileFire : RaceSound::MissileHit;
    }
    return std::nullopt;
}

bool HasCrossed(Milliseconds mark, const RaceClock& before, const RaceClock& after) {
    return before.PhaseTime() < mark && after.PhaseTime() >= mark;
}

}

RaceSounds::RaceSounds(std::size_t racerCount, std::size_t brickCount, std::size_t weaponEventCount)
    : wallCooldowns_(racerCount), shields_(racerCount) {
    events_.reserve((racerCount * kMostSoundsPerRacer) + kMostRaceWideSounds + brickCount +
                    weaponEventCount);
}

void RaceSounds::BeginTick() {
    events_.clear();
}

void RaceSounds::HearClock(const RaceClock& before, const RaceClock& after,
                           const std::vector<bool>& isPlayer) {
    if (before.Phase() == RacePhase::Intro && after.Phase() == RacePhase::Countdown) {
        Add(RaceSound::CountdownBeep, 0);
        for (std::size_t racer = 0; racer < isPlayer.size(); ++racer) {
            if (isPlayer[racer]) {
                Add(RaceSound::EngineStart, racer);
            }
        }
    }
    if (before.Phase() == RacePhase::Countdown && after.Phase() == RacePhase::Countdown &&
        std::ranges::any_of(kLaterBeeps,
                            [&](Milliseconds mark) { return HasCrossed(mark, before, after); })) {
        Add(RaceSound::CountdownBeep, 0);
    }
    if (before.Phase() == RacePhase::Countdown && after.Phase() == RacePhase::Racing) {
        Add(RaceSound::Go, 0);
    }
}

void RaceSounds::HearRacer(std::size_t racer, const Racer& car, bool isPlayer, bool isRacing,
                           Milliseconds elapsed) {
    const RacerEvents& events = car.Events();
    if (events.turboStarted) {
        Add(RaceSound::TurboStart, racer, *events.turboStarted);
    }
    if (events.turboFaded) {
        Add(RaceSound::TurboFade, racer, *events.turboFaded);
    }
    if (events.hasWarpEntered) {
        Add(RaceSound::WarpEnter, racer);
    }
    if (events.hasWarpLanded) {
        Add(RaceSound::WarpLand, racer);
    }
    if (isPlayer && events.hasDriftStarted) {
        Add(RaceSound::DriftStart, racer);
    }
    Milliseconds& cooldown = wallCooldowns_.at(racer);
    cooldown = std::max(cooldown - elapsed, Milliseconds{0});
    if (isRacing && car.Body().hasWallContact && cooldown == Milliseconds{0}) {
        Add(RaceSound::WallHit, racer);
        cooldown = kWallCooldown;
    }
    HearShield(racer, car.Status().shield);
}

void RaceSounds::HearFinish(std::size_t racer) {
    Add(RaceSound::Finish, racer);
}

void RaceSounds::HearBricks(std::span<const BrickEvent> events, bool areBricksAudible) {
    for (const BrickEvent& event : events) {
        if (!areBricksAudible && event.kind != BrickEventKind::WhiteCollected) {
            continue;
        }
        Add({.sound = BrickSound(event.kind),
             .racer = event.racer,
             .brick = event.brick,
             .whiteCount = event.whiteCount});
    }
}

void RaceSounds::HearWeapons(std::span<const WeaponEvent> events) {
    for (const WeaponEvent& event : events) {
        if (const std::optional<RaceSound> sound = WeaponSound(event)) {
            Add({.sound = *sound, .racer = event.racer, .position = event.position});
        }
    }
}

void RaceSounds::HearShield(std::size_t racer, const ShieldStatus& shield) {
    HeardShield& heard = shields_.at(racer);
    if (shield.absorbs != heard.absorbs) {
        Add({.sound = RaceSound::ShieldAbsorb,
             .racer = static_cast<std::uint8_t>(racer),
             .variant = shield.absorbSound});
    }
    if (heard.isUp && !shield.level && shield.raised == heard.raised) {
        Add(RaceSound::ShieldExpire, racer);
    }
    heard = {.raised = shield.raised, .absorbs = shield.absorbs, .isUp = shield.level.has_value()};
}

void RaceSounds::Add(RaceSound sound, std::size_t racer, TurboLevel level) {
    Add({.sound = sound, .racer = static_cast<std::uint8_t>(racer), .level = level});
}

void RaceSounds::Add(SoundEvent event) {
    RR_ASSERT(events_.size() < events_.capacity());
    events_.push_back(event);
}

}
