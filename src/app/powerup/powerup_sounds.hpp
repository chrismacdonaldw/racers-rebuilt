#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "app/loop_voice.hpp"
#include "app/positional_sound.hpp"
#include "audio/mixer.hpp"
#include "core/duration.hpp"
#include "core/vector3.hpp"
#include "sim/powerup/powerup_effect.hpp"
#include "sim/powerup/powerup_kind.hpp"
#include "sim/race/race_session.hpp"
#include "sim/race/random.hpp"

namespace rr::app {

struct EffectLoop {
    std::size_t effect = 0;
    float volume = 1.0F;

    friend bool operator==(const EffectLoop&, const EffectLoop&) = default;
};

/// `voice` is in the game's numbering, as from `DriverVoice`.
[[nodiscard]] std::optional<std::size_t> TauntSound(std::int32_t voice);

/// Lightning plays its loop at its source instead of its pose.
[[nodiscard]] std::optional<EffectLoop> LoopOf(sim::PowerupKind kind,
                                               const sim::EffectProgress& progress);

/// Only the nearest projectile of each kind plays it.
[[nodiscard]] std::optional<std::size_t> FlightLoopOf(sim::PowerupKind kind,
                                                      const sim::EffectProgress& progress);

class Crackle {
public:
    /// Returns the crackle's distance ahead of the bolt's source when one sounds this tick.
    [[nodiscard]] std::optional<float> Tick(bool isSustained, Milliseconds elapsed,
                                            sim::Random& random);

private:
    Milliseconds wait_{0};
};

/// `effects` is `GENERAL.SBK` in bank order and must outlive it. `taunts` has one entry per racer.
class PowerupSounds {
public:
    PowerupSounds(std::span<const audio::SoundId> effects,
                  std::vector<std::optional<std::size_t>> taunts, sim::Random random);

    void Hear(const sim::RaceSession& session, const Listener& listener, Milliseconds elapsed,
              audio::Mixer& mixer);

    /// Stops the loops until the next `Hear`.
    void Silence(audio::Mixer& mixer);

private:
    struct Source {
        Vector3 position;
        Vector3 forward;
    };

    struct SlotSound {
        LoopVoice loop;
        Crackle crackle;
    };

    void HearTaunts(const sim::RaceSession& session, const Listener& listener,
                    audio::Mixer& mixer) const;
    void HearEffects(const sim::RaceSession& session, const Listener& listener,
                     Milliseconds elapsed, audio::Mixer& mixer);
    void HearFlights(const sim::RaceSession& session, const Listener& listener,
                     audio::Mixer& mixer);
    void HearSpins(const sim::RaceSession& session, const Listener& listener, audio::Mixer& mixer);
    void PlayOnce(std::size_t effect, const Vector3& source, const Listener& listener,
                  audio::Mixer& mixer) const;

    std::span<const audio::SoundId> effects_;
    std::vector<std::optional<std::size_t>> taunts_;
    sim::Random random_;
    std::vector<SlotSound> slots_;
    std::array<LoopVoice, 4> flights_;
    std::vector<LoopVoice> spins_;
};

}
