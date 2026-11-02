#include "app/cutscene/cutscene_sounds.hpp"

#include <format>
#include <string>
#include <utility>

#include "assets/game_path.hpp"
#include "core/log.hpp"

namespace rr::app {

CutsceneSounds::CutsceneSounds(audio::Mixer& mixer, std::span<const assets::CutsceneSound> sounds,
                               std::vector<std::optional<audio::SoundId>> ids)
    : mixer_(&mixer), sounds_(sounds), ids_(std::move(ids)), voices_(sounds.size()) {}

void CutsceneSounds::Start(std::size_t sound) {
    std::optional<audio::VoiceId>& voice = voices_[sound];
    const std::optional<audio::SoundId> id = ids_[sound];
    if (!id || (voice && mixer_->IsPlaying(*voice))) {
        return;
    }
    const assets::CutsceneSound& played = sounds_[sound];
    const audio::SoundCue cue{
        .sound = *id,
        .mix = {.volume = played.volume, .pitch = played.frequencyScale, .pan = played.pan}};
    voice = played.isLooping ? mixer_->Loop(cue) : mixer_->Start(cue);
}

void CutsceneSounds::Stop(std::size_t sound) {
    if (std::optional<audio::VoiceId>& voice = voices_[sound]) {
        mixer_->Stop(*voice);
        voice.reset();
    }
}

void CutsceneSounds::StopAll() {
    for (std::size_t sound = 0; sound < voices_.size(); ++sound) {
        Stop(sound);
    }
}

Result<std::vector<std::optional<audio::SoundId>>> LoadCutsceneSounds(
    audio::Mixer& mixer, SoundBanks& banks, const assets::Vfs& vfs, std::string_view folder,
    const assets::CutsceneEffectsAsset& effects) {
    std::vector<std::optional<std::span<const audio::SoundId>>> loaded;
    for (const std::string& bank : effects.soundBanks) {
        const std::string path = std::format("{}/{}.SBK", folder, assets::ToUpperAscii(bank));
        Result<std::span<const audio::SoundId>> sounds = banks.Load(mixer, vfs, path);
        if (!sounds && sounds.error().code != ErrorCode::NotFound) {
            return std::unexpected(std::move(sounds.error()));
        }
        if (!sounds) {
            log::Warn("{} doesn't exist, so its cutscene sounds are silent", path);
        }
        loaded.push_back(sounds ? std::optional(*sounds) : std::nullopt);
    }
    std::vector<std::optional<audio::SoundId>> ids;
    for (const assets::CutsceneSound& sound : effects.sounds) {
        if (sound.bank >= loaded.size()) {
            return Fail(ErrorCode::InvalidData, std::format("sound {} plays from bank {}, which "
                                                            "doesn't exist",
                                                            sound.name, sound.bank));
        }
        const std::optional<std::span<const audio::SoundId>>& bank = loaded[sound.bank];
        if (bank && sound.sound >= bank->size()) {
            return Fail(ErrorCode::InvalidData,
                        std::format("sound {} plays sound {} of bank {}, which has {}", sound.name,
                                    sound.sound, sound.bank, bank->size()));
        }
        ids.push_back(bank ? std::optional((*bank)[sound.sound]) : std::nullopt);
    }
    return ids;
}

}
