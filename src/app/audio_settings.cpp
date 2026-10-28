#include "app/audio_settings.hpp"

namespace rr::app {

void DeclareAudioSettings(settings::Registry& registry) {
    registry.Declare(kMusicVolume);
    registry.Declare(kEffectsVolume);
    registry.Declare(kVoiceVolume);
    registry.Declare(kDevPlaySound);
}

void ApplyVolumes(const settings::Registry& registry, audio::Mixer& mixer) {
    mixer.SetVolume(audio::Bus::Music, static_cast<float>(registry.Get(kMusicVolume)));
    mixer.SetVolume(audio::Bus::Effects, static_cast<float>(registry.Get(kEffectsVolume)));
    mixer.SetVolume(audio::Bus::Voice, static_cast<float>(registry.Get(kVoiceVolume)));
}

}
