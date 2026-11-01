#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "app/loop_voice.hpp"
#include "app/positional_sound.hpp"
#include "assets/event_table.hpp"
#include "audio/mixer.hpp"
#include "core/duration.hpp"
#include "sim/race/random.hpp"
#include "sim/track/track_event.hpp"

namespace rr::app {

/// `soundId` counts from 3000, the first sound of `bank`.
[[nodiscard]] std::optional<audio::SoundId> AmbientSoundAt(std::span<const audio::SoundId> bank,
                                                           std::int32_t soundId);

/// Plays a track's `EVB` sound records on the position-path events the race fires.
class AmbientSounds {
public:
    AmbientSounds(std::span<const assets::SoundResource> records,
                  std::span<const audio::SoundId> bank, sim::Random random);

    void Hear(std::span<const sim::TrackEvent> events, const Listener& listener,
              Milliseconds elapsed, audio::Mixer& mixer);

    /// Stops the loops until the next `Hear`, keeping what runs.
    void Silence(audio::Mixer& mixer);

    [[nodiscard]] std::size_t RunningCount() const;

private:
    struct Ambient {
        assets::SoundResource record;
        std::optional<audio::SoundId> sound;
        bool isRunning = false;
        Vector3 position;
        Milliseconds wait{0};
        LoopVoice loop;
    };

    void Apply(Ambient& ambient, const sim::TrackEvent& event, audio::Mixer& mixer);
    void Begin(Ambient& ambient, const sim::TrackEvent& event, audio::Mixer& mixer);
    static void Finish(Ambient& ambient, audio::Mixer& mixer);
    void Update(Ambient& ambient, Milliseconds elapsed, audio::Mixer& mixer);
    void PlayOnce(const Ambient& ambient, audio::Mixer& mixer) const;
    [[nodiscard]] Milliseconds DrawWait();

    std::vector<Ambient> ambients_;
    sim::Random random_;
    Listener listener_;
};

}
