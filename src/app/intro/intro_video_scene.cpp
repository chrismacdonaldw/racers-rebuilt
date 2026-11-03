#include "app/intro/intro_video_scene.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

#include "app/frame_loop.hpp"
#include "app/intro/intro_videos.hpp"
#include "audio/decoded_file.hpp"
#include "core/log.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kRgbaBytes = 4;

assets::ImageAsset BlankPicture(const assets::AviVideo& video) {
    return {.widthPx = video.width,
            .heightPx = video.height,
            .rgba = std::vector<std::byte>(std::size_t{video.width} * video.height * kRgbaBytes)};
}

}

std::unique_ptr<IntroVideoScene> IntroVideoScene::Create(render::Renderer& renderer,
                                                         audio::Mixer& mixer,
                                                         const assets::Vfs& vfs,
                                                         std::size_t video) {
    const std::string_view name = kIntroVideos.at(video).file;
    Result<assets::FileData> file = vfs.Read(name);
    Result<assets::AviFile> avi =
        file.and_then([](const assets::FileData& data) { return assets::ParseAvi(data.Bytes()); });
    Result<video::VideoDecoder> decoder = avi.and_then(
        [](const assets::AviFile& parsed) { return video::VideoDecoder::Create(parsed.video); });
    Result<render::TextureId> texture = decoder.and_then([&](const video::VideoDecoder&) {
        return renderer.UploadTexture(BlankPicture(avi->video));
    });
    std::optional<Playback> playback;
    if (texture) {
        assets::ImageAsset picture = BlankPicture(avi->video);
        playback.emplace(Playback{.file = std::move(*file),
                                  .avi = std::move(*avi),
                                  .decoder = std::move(*decoder),
                                  .texture = *texture,
                                  .picture = std::move(picture)});
    } else {
        log::Warn("{}: {}", name, texture.error().message);
    }
    return std::unique_ptr<IntroVideoScene>(
        new IntroVideoScene(renderer, mixer, std::move(playback), video));
}

IntroVideoScene::IntroVideoScene(render::Renderer& renderer, audio::Mixer& mixer,
                                 std::optional<Playback> playback, std::size_t video)
    : renderer_(&renderer), mixer_(&mixer), playback_(std::move(playback)), video_(video) {}

void IntroVideoScene::Enter() {
    if (!playback_ || !playback_->avi.audio) {
        return;
    }
    const assets::AviAudio& audio = *playback_->avi.audio;
    const Result<void> played = mixer_->PlayMusic(
        audio::SampleStream({.channelCount = audio.channels, .sampleRateHz = audio.sampleRate},
                            audio.samples),
        Milliseconds{0});
    if (!played) {
        log::Warn("{}: {}", kIntroVideos.at(video_).file, played.error().message);
    }
    hasSoundtrack_ = played.has_value() && mixer_->HasDevice();
}

void IntroVideoScene::Exit() {
    mixer_->StopMusic();
}

std::optional<std::string> IntroVideoScene::TakeNextScene() {
    return std::exchange(next_, std::nullopt);
}

void IntroVideoScene::HandleEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_UP && kIntroVideos.at(video_).isSkippable) {
        isSkipAsked_ = true;
    }
}

void IntroVideoScene::Tick([[maybe_unused]] const sim::InputFrame& input) {
    elapsed_ += std::chrono::duration_cast<Microseconds>(kSimTick);
    if (!next_ && (isSkipAsked_ || !playback_)) {
        next_ = SceneAfterIntroVideo(video_);
    }
}

void IntroVideoScene::Snapshot(sim::RenderSnapshot& out) const {
    out = {};
    out.simStateHash = static_cast<std::uint64_t>(elapsed_.count());
}

void IntroVideoScene::Render([[maybe_unused]] const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    if (playback_) {
        Playback& playback = *playback_;
        ShowCurrentFrame(playback);
        sprites_.push_back({.texture = playback.texture,
                            .bounds = {.width = render::kUiWidth, .height = render::kUiHeight}});
    }
    const Result<void> drawn = renderer_->Draw({.sprites = sprites_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

std::optional<Microseconds> IntroVideoScene::PlayedTime() {
    return hasSoundtrack_ ? clock_.Now(elapsed_, mixer_->MusicTime()) : elapsed_;
}

void IntroVideoScene::ShowCurrentFrame(Playback& playback) {
    const std::vector<std::span<const std::byte>>& frames = playback.avi.video.frames;
    const std::optional<Microseconds> played = PlayedTime();
    const std::size_t shown = played ? assets::FrameAt(playback.avi.video, *played) : frames.size();
    if (shown >= frames.size()) {
        if (!next_) {
            next_ = SceneAfterIntroVideo(video_);
        }
        return;
    }
    if (decoded_ > shown) {
        return;
    }
    for (; decoded_ <= shown; ++decoded_) {
        const Result<void> decoded = playback.decoder.Decode(frames[decoded_]);
        if (!decoded) {
            log::Warn("{} frame {}: {}", kIntroVideos.at(video_).file, decoded_,
                      decoded.error().message);
        }
    }
    const std::span<const std::uint8_t> picture = playback.decoder.Picture();
    std::memcpy(playback.picture.rgba.data(), picture.data(),
                std::min(picture.size(), playback.picture.rgba.size()));
    const Result<void> updated = renderer_->UpdateTexture(playback.texture, playback.picture);
    if (!updated) {
        log::Error("{}", updated.error().message);
    }
}

}
