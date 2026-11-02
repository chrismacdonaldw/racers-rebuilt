#include "app/music_folder_source.hpp"

#include <array>
#include <format>
#include <memory>
#include <system_error>
#include <utility>

#include "app/loop_sidecar.hpp"
#include "audio/decoded_file.hpp"
#include "audio/music_catalog.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"

namespace rr::app {
namespace {

constexpr std::array<std::string_view, 4> kExtensions{".ogg", ".flac", ".mp3", ".wav"};

audio::LoopPoints ReadLoopPoints(const std::filesystem::path& sidecar,
                                 const audio::PcmStream& stream) {
    Result<audio::LoopPoints> loop = ReadLoopSidecar(sidecar);
    if (loop && loop->startFrame < stream.FrameCount()) {
        return *loop;
    }
    log::Warn("{}, looping the whole track",
              loop ? std::format("{}: loop_start is past the end", Utf8FromPath(sidecar))
                   : loop.error().message);
    return {};
}

}

MusicFolderSource::MusicFolderSource(std::filesystem::path folder) : folder_(std::move(folder)) {}

std::string MusicFolderSource::Name() const {
    return Utf8FromPath(folder_);
}

Result<audio::MusicTrack> MusicFolderSource::Open(std::string_view id) const {
    std::error_code error;
    for (const std::string_view extension : kExtensions) {
        const std::filesystem::path path =
            folder_ / PathFromUtf8(std::format("{}{}", id, extension));
        if (!std::filesystem::is_regular_file(path, error)) {
            continue;
        }
        Result<std::unique_ptr<audio::PcmStream>> stream = audio::DecodeFile(path);
        if (!stream) {
            return std::unexpected(std::move(stream.error()));
        }
        const audio::LoopPoints loop =
            ReadLoopPoints(folder_ / PathFromUtf8(std::format("{}.toml", id)), **stream);
        return audio::MusicTrack{.stream = std::move(*stream), .loop = loop};
    }
    return Fail(ErrorCode::NotFound, std::format("no replacement for {}", id));
}

audio::MusicPlayer CreateMusicPlayer(audio::Mixer& mixer, const assets::Vfs& vfs,
                                     const std::filesystem::path& musicFolder,
                                     std::span<const mods::Mod* const> enabledMods) {
    Result<audio::MusicCatalog> catalog = audio::MusicCatalog::Scan(vfs);
    log::Info("{} music tracks, replacements read from {}", catalog ? catalog->Ids().size() : 0,
              Utf8FromPath(musicFolder));
    std::vector<std::unique_ptr<audio::MusicSource>> sources;
    sources.push_back(std::make_unique<MusicFolderSource>(musicFolder));
    for (const mods::Mod* mod : enabledMods) {
        sources.push_back(std::make_unique<MusicFolderSource>(mod->MusicFolder()));
    }
    sources.push_back(std::make_unique<audio::OriginalTrackSource>(vfs));
    return {mixer, std::move(sources)};
}

}
