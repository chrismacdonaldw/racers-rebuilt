#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "audio/music_source.hpp"

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

}
