#include "audio/music_player.hpp"

#include <format>
#include <utility>

#include "assets/game_path.hpp"
#include "audio/looping_stream.hpp"
#include "core/log.hpp"

namespace rr::audio {

MusicPlayer::MusicPlayer(Mixer& mixer, std::vector<std::unique_ptr<MusicSource>> sources)
    : mixer_(&mixer), sources_(std::move(sources)) {}

Result<void> MusicPlayer::Play(std::string_view id, Repeat repeat) {
    if (id == current_) {
        return {};
    }
    Result<MusicTrack> track = Find(id);
    if (!track) {
        return std::unexpected(std::move(track.error()));
    }
    std::unique_ptr<PcmStream> stream = std::move(track->stream);
    if (repeat == Repeat::Loop) {
        stream = std::make_unique<LoopingStream>(std::move(stream), track->loop);
    }
    Result<void> played = mixer_->PlayMusic(std::move(stream), kMusicCrossfade);
    if (played) {
        current_ = id;
    }
    return played;
}

void MusicPlayer::Stop() {
    mixer_->StopMusic(kMusicCrossfade);
    current_.clear();
}

Result<MusicTrack> MusicPlayer::Find(std::string_view id) const {
    Result<std::string> name = assets::NormalizeGameName(id);
    if (!name) {
        return Fail(ErrorCode::InvalidData,
                    std::format("music track {}: {}", id, name.error().message));
    }
    for (const std::unique_ptr<MusicSource>& source : sources_) {
        Result<MusicTrack> track = source->Open(id);
        if (track) {
            log::Info("music: {} from {}", id, source->Name());
            return track;
        }
        if (track.error().code != ErrorCode::NotFound) {
            log::Warn("{}, trying the next source", track.error().message);
        }
    }
    return Fail(ErrorCode::NotFound, std::format("no source has the music track {}", id));
}

}
