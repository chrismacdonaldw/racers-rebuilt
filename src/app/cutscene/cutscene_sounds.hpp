#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "app/sound_banks.hpp"
#include "assets/cutscene_effects.hpp"
#include "assets/vfs.hpp"
#include "audio/mixer.hpp"
#include "core/result.hpp"

namespace rr::app {

/// `sounds` and the mixer must outlive it. `ids[i]` plays `sounds[i]`, and an unset one is silent.
class CutsceneSounds {
public:
    CutsceneSounds(audio::Mixer& mixer, std::span<const assets::CutsceneSound> sounds,
                   std::vector<std::optional<audio::SoundId>> ids);

    /// Does nothing while the sound still plays, so a second start never restarts it.
    void Start(std::size_t sound);
    void Stop(std::size_t sound);
    void StopAll();

private:
    audio::Mixer* mixer_;
    std::span<const assets::CutsceneSound> sounds_;
    std::vector<std::optional<audio::SoundId>> ids_;
    std::vector<std::optional<audio::VoiceId>> voices_;
};

/// Loads the `CEB`'s banks from `folder`. A missing bank, as in `WINCAR`, leaves its sounds unset.
[[nodiscard]] Result<std::vector<std::optional<audio::SoundId>>> LoadCutsceneSounds(
    audio::Mixer& mixer, SoundBanks& banks, const assets::Vfs& vfs, std::string_view folder,
    const assets::CutsceneEffectsAsset& effects);

}
