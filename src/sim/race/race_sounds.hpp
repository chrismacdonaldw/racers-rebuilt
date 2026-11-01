#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/duration.hpp"
#include "core/vector3.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/turbo.hpp"
#include "sim/powerup/brick_field.hpp"
#include "sim/powerup/weapon_events.hpp"
#include "sim/race/race_clock.hpp"

namespace rr::sim {

enum class RaceSound : std::uint8_t {
    CountdownBeep,
    Go,
    EngineStart,
    TurboStart,
    TurboFade,
    WarpEnter,
    WarpLand,
    DriftStart,
    WallHit,
    CarScrape,
    Finish,
    BrickRespawn,
    BrickCollect,
    BrickSwap,
    WhiteBrickCollect,
    ShieldAbsorb,
    ShieldExpire,
    CannonFire,
    CannonHit,
    MissileFire,
    MissileHit,
};

/// `racer` means nothing for countdown sounds, and the rest only for the sounds that use them.
struct SoundEvent {
    RaceSound sound = RaceSound::CountdownBeep;
    std::uint8_t racer = 0;
    TurboLevel level = TurboLevel::Weak;
    std::uint32_t brick = 0;
    std::uint8_t whiteCount = 0;
    std::uint8_t variant = 0;
    Vector3 position;

    friend bool operator==(const SoundEvent&, const SoundEvent&) = default;
};

/// Only reads the race, so it never changes the outcome.
class RaceSounds {
public:
    RaceSounds(std::size_t racerCount, std::size_t brickCount, std::size_t weaponEventCount = 0);

    void BeginTick();

    /// `before` is the clock as the tick started.
    void HearClock(const RaceClock& before, const RaceClock& after,
                   const std::vector<bool>& isPlayer);

    void HearRacer(std::size_t racer, const Racer& car, bool isPlayer, bool isRacing,
                   Milliseconds elapsed);

    void HearFinish(std::size_t racer);

    /// `areBricksAudible` gates the bricks' own sounds, not the white pickup sound.
    void HearBricks(std::span<const BrickEvent> events, bool areBricksAudible);

    void HearWeapons(std::span<const WeaponEvent> events);

    [[nodiscard]] std::span<const SoundEvent> Events() const { return events_; }

private:
    struct HeardShield {
        std::uint32_t raised = 0;
        std::uint32_t absorbs = 0;
        bool isUp = false;
    };

    void HearShield(std::size_t racer, const ShieldStatus& shield);
    void Add(RaceSound sound, std::size_t racer, TurboLevel level = TurboLevel::Weak);
    void Add(SoundEvent event);

    std::vector<SoundEvent> events_;
    std::vector<Milliseconds> wallCooldowns_;
    std::vector<HeardShield> shields_;
};

}
