#pragma once

#include <optional>

#include "audio/mixer.hpp"
#include "audio/voice_mix.hpp"

namespace rr::app {

/// Plays one sound on repeat, remixing it while the sound stays the same.
class LoopVoice {
public:
    void Play(audio::Mixer& mixer, audio::SoundId sound, const audio::VoiceMix& mix);
    void Stop(audio::Mixer& mixer);

private:
    std::optional<audio::VoiceId> voice_;
    audio::SoundId sound_;
};

}
