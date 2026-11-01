#include "app/loop_voice.hpp"

namespace rr::app {

void LoopVoice::Play(audio::Mixer& mixer, audio::SoundId sound, const audio::VoiceMix& mix) {
    if (voice_ && sound_.index == sound.index) {
        mixer.Mix(*voice_, mix);
        return;
    }
    Stop(mixer);
    voice_ = mixer.Loop({.sound = sound, .mix = mix});
    sound_ = sound;
}

void LoopVoice::Stop(audio::Mixer& mixer) {
    if (voice_) {
        mixer.Stop(*voice_);
        voice_.reset();
    }
}

}
