#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/duration.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/turbo.hpp"
#include "sim/race/race_clock.hpp"

namespace rr::sim {

enum class RaceSound : std::uint8_t {
    CountdownBeep,
    Go,
    EngineStart,
    TurboStart,
    TurboFade,
    DriftStart,
    WallHit,
    Finish,
};

/// `racer` and `level` mean nothing for the countdown sounds, and `level` only for turbo ones.
struct SoundEvent {
    RaceSound sound = RaceSound::CountdownBeep;
    std::uint8_t racer = 0;
    TurboLevel level = TurboLevel::Weak;

    friend bool operator==(const SoundEvent&, const SoundEvent&) = default;
};

/// Only reads the race, so it never changes the outcome.
class RaceSounds {
public:
    explicit RaceSounds(std::size_t racerCount);

    void BeginTick();

    /// `before` is the clock as the tick started.
    void HearClock(const RaceClock& before, const RaceClock& after,
                   const std::vector<bool>& isPlayer);

    void HearRacer(std::size_t racer, const Racer& car, bool isPlayer, bool isRacing,
                   Milliseconds elapsed);

    void HearFinish(std::size_t racer);

    [[nodiscard]] std::span<const SoundEvent> Events() const { return events_; }

private:
    void Add(RaceSound sound, std::size_t racer, TurboLevel level = TurboLevel::Weak);

    std::vector<SoundEvent> events_;
    std::vector<Milliseconds> wallCooldowns_;
};

}
