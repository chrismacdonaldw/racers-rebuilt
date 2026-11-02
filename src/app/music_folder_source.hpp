#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include "assets/vfs.hpp"
#include "audio/mixer.hpp"
#include "audio/music_player.hpp"
#include "audio/music_source.hpp"
#include "mods/mod_list.hpp"

namespace rr::app {

/// `<id>.ogg`, `.flac`, `.mp3` or `.wav`, with optional loop points in `<id>.toml`.
class MusicFolderSource final : public audio::MusicSource {
public:
    explicit MusicFolderSource(std::filesystem::path folder);

    [[nodiscard]] std::string Name() const override;
    [[nodiscard]] Result<audio::MusicTrack> Open(std::string_view id) const override;

private:
    std::filesystem::path folder_;
};

/// Plays replacements from `musicFolder`, then each enabled mod's, then the original tracks.
[[nodiscard]] audio::MusicPlayer CreateMusicPlayer(audio::Mixer& mixer, const assets::Vfs& vfs,
                                                   const std::filesystem::path& musicFolder,
                                                   std::span<const mods::Mod* const> enabledMods);

}
