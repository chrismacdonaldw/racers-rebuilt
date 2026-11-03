#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/intro/soundtrack_clock.hpp"
#include "app/scene.hpp"
#include "assets/avi_file.hpp"
#include "assets/image_asset.hpp"
#include "assets/vfs.hpp"
#include "audio/mixer.hpp"
#include "core/duration.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "video/video_decoder.hpp"

namespace rr::app {

class IntroVideoScene final : public Scene {
public:
    /// `video` indexes `kIntroVideos`. A video that can't be read is skipped on the first tick.
    [[nodiscard]] static std::unique_ptr<IntroVideoScene> Create(render::Renderer& renderer,
                                                                 audio::Mixer& mixer,
                                                                 const assets::Vfs& vfs,
                                                                 std::size_t video);

    void Enter() override;
    void Exit() override;
    [[nodiscard]] SceneMusic Music() const override { return {.start = MusicStart::Keep}; }
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    void HandleEvent(const SDL_Event& event) override;
    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    struct Playback {
        assets::FileData file;
        assets::AviFile avi;
        video::VideoDecoder decoder;
        render::TextureId texture;
        assets::ImageAsset picture;
    };

    IntroVideoScene(render::Renderer& renderer, audio::Mixer& mixer,
                    std::optional<Playback> playback, std::size_t video);

    [[nodiscard]] std::optional<Microseconds> PlayedTime();
    void ShowCurrentFrame(Playback& playback);

    render::Renderer* renderer_;
    audio::Mixer* mixer_;
    std::optional<Playback> playback_;
    std::size_t video_;
    Microseconds elapsed_{0};
    std::size_t decoded_ = 0;
    bool hasSoundtrack_ = false;
    SoundtrackClock clock_;
    bool isSkipAsked_ = false;
    std::optional<std::string> next_;
    std::vector<render::SpriteDraw> sprites_;
};

}
