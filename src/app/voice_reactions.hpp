#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "app/positional_sound.hpp"
#include "app/sound_banks.hpp"
#include "assets/vfs.hpp"
#include "audio/mixer.hpp"
#include "core/duration.hpp"
#include "sim/physics/racer.hpp"
#include "sim/race/race_session.hpp"
#include "sim/race/random.hpp"

namespace rr::app {

inline constexpr std::int32_t kBuiltCarVoice = 1000;

/// The driver's first line in the track's circuit bank, counted from 1100 as the game does.
[[nodiscard]] std::int32_t DriverVoice(std::int32_t voiceBank);

[[nodiscard]] std::int32_t BuiltCarVoice(std::size_t slot);

/// Nothing for a built car's voice or one past the bank's six drivers.
[[nodiscard]] std::optional<std::size_t> CircuitDriver(std::int32_t voice);

/// Empty when the track has no circuit voices.
[[nodiscard]] std::span<const audio::SoundId> LoadCircuitVoices(audio::Mixer& mixer,
                                                                SoundBanks& banks,
                                                                const assets::Vfs& vfs,
                                                                std::string_view track);

/// Keeps each racer quiet for 5 to 9 s after a reaction, and from the start.
class ReactionTimers {
public:
    ReactionTimers(std::size_t racerCount, sim::Random random);

    void Tick(Milliseconds elapsed);

    /// Returns which of the driver's 12 lines to say, or nothing while the racer is quiet.
    [[nodiscard]] std::optional<std::size_t> React(std::size_t racer, sim::VoiceReaction reaction);

private:
    Milliseconds Roll();

    sim::Random random_;
    std::vector<Milliseconds> quiet_;
};

/// `lines` is the track's circuit bank and must outlive it. `voices` has one entry per racer.
class VoiceReactions {
public:
    VoiceReactions(std::span<const audio::SoundId> lines, std::vector<std::int32_t> voices,
                   sim::Random random);

    void Hear(const sim::RaceSession& session, const Listener& listener, Milliseconds elapsed,
              audio::Mixer& mixer);

private:
    std::span<const audio::SoundId> lines_;
    std::vector<std::int32_t> voices_;
    ReactionTimers timers_;
};

}
