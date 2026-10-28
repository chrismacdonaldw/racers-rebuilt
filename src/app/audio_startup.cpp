#include "app/audio_startup.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "app/audio_settings.hpp"
#include "assets/game_path.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::uint32_t kSilentMixRateHz = 22050;

Result<void> PlaySound(audio::Mixer& mixer, const assets::Vfs& vfs, std::string_view path) {
    Result<std::string> gamePath = assets::NormalizeGamePath(path);
    Result<audio::SoundId> sound =
        gamePath.and_then([&](const std::string& normalized) { return vfs.Read(normalized); })
            .and_then([&](assets::FileData file) { return mixer.AddSound(std::move(file)); });
    if (!sound) {
        return std::unexpected(std::move(sound.error()));
    }
    if (gamePath->ends_with(".TUN")) {
        return mixer.PlayMusic(*sound);
    }
    const std::array cues{audio::SoundCue{.sound = *sound}};
    mixer.Play(cues);
    return {};
}

}

Result<audio::Mixer> StartAudio(const settings::Registry& registry, const assets::Vfs& vfs) {
    Result<audio::Mixer> mixer = audio::Mixer::Create();
    if (!mixer) {
        log::Warn("{}, continuing without sound", mixer.error().message);
        mixer = audio::Mixer::CreateHeadless(kSilentMixRateHz);
    }
    if (!mixer) {
        return mixer;
    }
    ApplyVolumes(registry, *mixer);
    const std::string devSound = registry.Get(kDevPlaySound);
    if (!devSound.empty()) {
        const Result<void> played = PlaySound(*mixer, vfs, devSound);
        if (played) {
            log::Info("{}: playing {}", kDevPlaySound.key, devSound);
        } else {
            log::Warn("{}: {}", kDevPlaySound.key, played.error().message);
        }
    }
    return mixer;
}

}
