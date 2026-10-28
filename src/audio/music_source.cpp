#include "audio/music_source.hpp"

#include <format>
#include <utility>

#include "assets/game_path.hpp"
#include "audio/alp.hpp"

namespace rr::audio {
namespace {

class AlpTrack final : public PcmStream {
public:
    AlpTrack(assets::FileData file, AlpSound sound) : file_(std::move(file)), decoder_(sound) {}

    [[nodiscard]] PcmFormat Format() const override { return decoder_.Format(); }
    [[nodiscard]] std::uint64_t FrameCount() const override { return decoder_.FrameCount(); }
    [[nodiscard]] std::uint64_t Cursor() const override { return decoder_.Cursor(); }

    std::uint64_t Read(std::span<std::int16_t> interleaved) noexcept override {
        return decoder_.Read(interleaved);
    }
    void Seek(std::uint64_t frame) noexcept override { decoder_.Seek(frame); }

private:
    assets::FileData file_;
    AlpDecoder decoder_;
};

}

Result<MusicTrack> OriginalTrackSource::Open(std::string_view id) const {
    Result<assets::FileData> file =
        assets::NormalizeGamePath(std::format("{}.tun", id)).and_then([&](const std::string& path) {
            return vfs_->Read(path);
        });
    if (!file) {
        return std::unexpected(std::move(file.error()));
    }
    Result<AlpSound> sound = ParseAlp(file->Bytes());
    if (!sound) {
        return Fail(ErrorCode::InvalidData, std::format("{}.tun: {}", id, sound.error().message));
    }
    return MusicTrack{.stream = std::make_unique<AlpTrack>(std::move(*file), *sound), .loop = {}};
}

}
