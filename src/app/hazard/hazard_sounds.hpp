#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "app/loop_voice.hpp"
#include "app/positional_sound.hpp"
#include "audio/mixer.hpp"
#include "core/duration.hpp"
#include "sim/hazard/hazards.hpp"
#include "sim/race/race_session.hpp"
#include "sim/race/random.hpp"

namespace rr::app {

/// `bank` holds the track's own sounds in bank order and must outlive it.
class HazardSounds {
public:
    HazardSounds(std::span<const audio::SoundId> bank, sim::Random random);

    void Hear(const sim::RaceSession& session, const Listener& listener, Milliseconds elapsed,
              audio::Mixer& mixer);

    /// Stops the loops until the next `Hear`.
    void Silence(audio::Mixer& mixer);

private:
    struct SlotLoop {
        LoopVoice voice;
        Milliseconds wobble{0};
        float depth = 0.0F;
    };

    void Loop(SlotLoop& loop, const sim::RaceSession& session, const sim::HazardState& state,
              const Listener& listener, Milliseconds elapsed, audio::Mixer& mixer);
    [[nodiscard]] float Wobble(SlotLoop& loop, Milliseconds elapsed);
    void Play(std::int32_t soundId, SlotLoop& loop, const Vector3& source, float pitch,
              const Listener& listener, audio::Mixer& mixer) const;

    std::span<const audio::SoundId> bank_;
    sim::Random random_;
    std::vector<SlotLoop> loops_;
};

}
