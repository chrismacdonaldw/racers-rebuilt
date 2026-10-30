#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/extensions.hpp"
#include "app/frame_loop.hpp"
#include "app/game_boot.hpp"
#include "app/sound_banks.hpp"
#include "assets/vfs.hpp"
#include "audio/mixer.hpp"
#include "audio/music_player.hpp"
#include "core/result.hpp"
#include "overlay/overlay.hpp"
#include "platform/event_pump.hpp"
#include "platform/frame_pacer.hpp"
#include "platform/sdl_context.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"
#include "settings/registry.hpp"

namespace rr::app {

// NOLINTNEXTLINE(bugprone-exception-escape)
class Application {
public:
    [[nodiscard]] static Result<Application> Create(GameBoot boot);

    [[nodiscard]] Result<void> Run();

private:
    struct Parts {
        platform::SdlContext sdl;
        settings::Registry settings;
        std::filesystem::path settingsFile;
        Extensions extensions;
        assets::Vfs vfs;
        audio::Mixer mixer;
        platform::Window window;
        render::Renderer renderer;
        platform::FramePacer pacer;
        platform::PacingConfig pacing;
        std::optional<std::string> scene;
        std::optional<std::filesystem::path> recordFile;
        std::optional<std::filesystem::path> replayFile;
        std::string startupSummary;
    };

    using SceneFactory = std::function<Result<std::unique_ptr<Scene>>()>;

    explicit Application(Parts parts);

    [[nodiscard]] Result<std::unique_ptr<Scene>> CreateScene(
        const std::optional<std::string>& scene);
    [[nodiscard]] Result<std::unique_ptr<Scene>> CreateRace(const std::string& track);
    [[nodiscard]] Result<void> OpenScene(const std::optional<std::string>& scene);
    [[nodiscard]] Result<void> SwitchScene(const SceneFactory& create);
    [[nodiscard]] std::vector<overlay::SceneChoice> SceneChoices();
    void RunFrames(overlay::Overlay& menu, audio::MusicPlayer& music);
    void OpenNextScene();
    void CyclePacing();
    void ApplyVideo();
    void Persist(const std::vector<std::string>& keys);

    platform::SdlContext sdl_;
    settings::Registry settings_;
    std::filesystem::path settingsFile_;
    Extensions extensions_;
    assets::Vfs vfs_;
    audio::Mixer mixer_;
    SoundBanks soundBanks_;
    platform::Window window_;
    render::Renderer renderer_;
    platform::EventPump events_;
    platform::FramePacer pacer_;
    platform::PacingConfig pacing_;
    std::optional<std::string> scene_;
    std::uint64_t seed_ = 0;
    bool isQuitting_ = false;
    std::optional<std::filesystem::path> recordFile_;
    std::optional<std::filesystem::path> replayFile_;
    std::string startupSummary_;
    FrameLoop frameLoop_;
};

}
