#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/vfs.hpp"
#include "audio/mixer.hpp"
#include "core/result.hpp"

namespace rr::app {

/// Adds each sound bank to the mixer once and keeps it for the mixer's lifetime.
class SoundBanks {
public:
    /// `path` names a bank such as `GAMEDATA/COMMON/GENERAL.SBK`. Sounds come in bank order.
    [[nodiscard]] Result<std::span<const audio::SoundId>> Load(audio::Mixer& mixer,
                                                               const assets::Vfs& vfs,
                                                               std::string_view path);

private:
    struct Bank {
        std::string path;
        std::vector<audio::SoundId> sounds;
    };

    std::vector<Bank> banks_;
};

/// The ambient bank of a race track folder such as `RACEC0R0`.
[[nodiscard]] std::string AmbientBankPath(std::string_view track);

/// A race track folder's `LEGOMSC`, as `MusicCatalog` ids.
[[nodiscard]] Result<std::vector<std::string>> LoadMusicList(const assets::Vfs& vfs,
                                                             std::string_view track);

}
