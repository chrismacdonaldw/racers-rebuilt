#include "app/application.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/audio_settings.hpp"
#include "app/audio_startup.hpp"
#include "app/extensions.hpp"
#include "app/music_folder_source.hpp"
#include "app/startup_settings.hpp"
#include "app/test_scene.hpp"
#include "app/track_viewer_scene.hpp"
#include "app/video_settings.hpp"
#include "assets/loader_pool.hpp"
#include "assets/track_loader.hpp"
#include "audio/music_catalog.hpp"
#include "audio/music_player.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "overlay/overlay.hpp"
#include "platform/clock.hpp"

namespace rr::app {
namespace {

constexpr const char* kWindowTitle = "racers-rebuilt";
constexpr int kWindowWidthPx = 1280;
constexpr int kWindowHeightPx = 720;
constexpr std::string_view kMusicFolderName = "music";
constexpr SDL_Keycode kCyclePacingKey = SDLK_F9;
constexpr SDL_Keycode kToggleOverlayKey = SDLK_F1;

constexpr std::array kPacingPresets{
    platform::PacingConfig{.mode = platform::PresentMode::Vsync},
    platform::PacingConfig{.mode = platform::PresentMode::Capped, .fpsCap = 30},
    platform::PacingConfig{.mode = platform::PresentMode::Capped, .fpsCap = 60},
    platform::PacingConfig{.mode = platform::PresentMode::Capped, .fpsCap = 144},
    platform::PacingConfig{.mode = platform::PresentMode::Uncapped},
};

bool IsSamePacing(const platform::PacingConfig& a, const platform::PacingConfig& b) {
    return a.mode == b.mode && (a.mode != platform::PresentMode::Capped || a.fpsCap == b.fpsCap);
}

platform::PacingConfig NextPacingPreset(const platform::PacingConfig& current) {
    const auto match = std::ranges::find_if(
        kPacingPresets,
        [&](const platform::PacingConfig& preset) { return IsSamePacing(preset, current); });
    const std::size_t next = match == kPacingPresets.end()
                                 ? 0
                                 : (static_cast<std::size_t>(match - kPacingPresets.begin()) + 1) %
                                       kPacingPresets.size();
    return kPacingPresets.at(next);
}

void LogPacing(const platform::PacingConfig& pacing) {
    switch (pacing.mode) {
        case platform::PresentMode::Vsync:
            log::Info("pacing: VSync");
            break;
        case platform::PresentMode::Capped:
            log::Info("pacing: capped at {} FPS", pacing.fpsCap);
            break;
        case platform::PresentMode::Uncapped:
            log::Info("pacing: uncapped");
            break;
    }
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

void PlaySceneMusic(audio::MusicPlayer& music, const Scene& scene) {
    if (scene.MusicTrack().empty()) {
        music.Stop();
        return;
    }
    Result<void> played = music.Play(scene.MusicTrack());
    if (!played) {
        log::Warn("{}", played.error().message);
    }
}

Result<platform::FramePacer> CreatePacer(const platform::PacingConfig& pacing) {
    Result<platform::FrameTimer> timer = platform::FrameTimer::Create();
    if (!timer) {
        return std::unexpected(std::move(timer.error()));
    }
    LogPacing(pacing);
    return platform::FramePacer(std::move(*timer), pacing);
}

}

Result<Application> Application::Create(GameBoot boot) {
    Result<platform::Window> window = platform::Window::Create(
        {.title = kWindowTitle, .widthPx = kWindowWidthPx, .heightPx = kWindowHeightPx});
    if (!window) {
        return std::unexpected(std::move(window.error()));
    }
    boot.startup.EndPhase("window", platform::Now());

    const platform::PacingConfig pacing = PacingFromSettings(boot.settings);
    Result<render::Renderer> renderer = render::Renderer::Create(*window, pacing.mode);
    Result<platform::FramePacer> pacer =
        renderer.and_then([&](const render::Renderer&) { return CreatePacer(pacing); });
    if (!pacer) {
        return std::unexpected(std::move(pacer.error()));
    }
    renderer->SetResolutionScale(static_cast<float>(boot.settings.Get(kResolutionScaleSetting)));
    boot.startup.EndPhase("renderer", platform::Now());

    Result<audio::Mixer> mixer = StartAudio(boot.settings, boot.vfs);
    if (!mixer) {
        return std::unexpected(std::move(mixer.error()));
    }
    boot.startup.EndPhase("audio", platform::Now());
    std::string startupSummary = boot.startup.Summary();
    log::Info("{}", startupSummary);

    return Application({.sdl = std::move(boot.sdl),
                        .settings = std::move(boot.settings),
                        .settingsFile = std::move(boot.settingsFile),
                        .extensions = std::move(boot.extensions),
                        .vfs = std::move(boot.vfs),
                        .mixer = std::move(*mixer),
                        .window = std::move(*window),
                        .renderer = std::move(*renderer),
                        .pacer = std::move(*pacer),
                        .pacing = pacing,
                        .track = std::move(boot.commandLine.track),
                        .startupSummary = std::move(startupSummary)});
}

Result<void> Application::Run() {
    Result<std::unique_ptr<overlay::Overlay>> menu = overlay::Overlay::Create(window_, renderer_);
    Result<void> opened = menu.and_then([&](const auto&) { return OpenScene(track_); });
    if (!opened) {
        return opened;
    }
    events_.Observe([&menu](const SDL_Event& event) { (*menu)->HandleEvent(event); });
    const std::array scenes{overlay::SceneChoice{.name = "Test scene", .open = [this] {
                                                     const Result<void> reopened =
                                                         OpenScene(std::nullopt);
                                                     if (!reopened) {
                                                         log::Error("{}", reopened.error().message);
                                                     }
                                                 }}};
    audio::MusicPlayer music =
        CreateMusicPlayer(mixer_, vfs_, settingsFile_.parent_path() / kMusicFolderName,
                          extensions_.mods.Enabled(settings_));
    AttachEnhancements(extensions_, frameLoop_.Hooks(), settings_);
    settings_.Observe([this](std::string_view key) {
        frameLoop_.Hooks().Emit(mods::SettingChanged{.key = key, .settings = settings_});
        if (key.starts_with("video.")) {
            ApplyVideo();
        }
        if (key.starts_with("audio.")) {
            ApplyVolumes(settings_, mixer_);
        }
    });

    Nanoseconds lastFrame = platform::Now();
    FrameStats frame;
    while (!events_.IsQuitRequested()) {
        events_.Poll();
        if (events_.WasKeyPressed(kToggleOverlayKey)) {
            (*menu)->Toggle();
        }
        if (events_.WasKeyPressed(kCyclePacingKey)) {
            CyclePacing();
        }
        const Nanoseconds now = platform::Now();
        Persist(
            (*menu)->Build(settings_,
                           {.frameTime = std::chrono::duration_cast<FloatSeconds>(now - lastFrame),
                            .ticksPerFrame = frame.tickCount,
                            .startupSummary = startupSummary_},
                           scenes));
        lastFrame = now;
        frame = frameLoop_.RunFrame(now);
        if (frame.isNewScene && frameLoop_.Scenes().Top() != nullptr) {
            PlaySceneMusic(music, *frameLoop_.Scenes().Top());
        }
        pacer_.WaitForNextFrame();
    }
    log::Info("quit requested, shutting down");
    return {};
}

Application::Application(Parts parts)
    : sdl_(std::move(parts.sdl)),
      settings_(std::move(parts.settings)),
      settingsFile_(std::move(parts.settingsFile)),
      extensions_(std::move(parts.extensions)),
      vfs_(std::move(parts.vfs)),
      mixer_(std::move(parts.mixer)),
      window_(std::move(parts.window)),
      renderer_(std::move(parts.renderer)),
      pacer_(std::move(parts.pacer)),
      pacing_(parts.pacing),
      track_(std::move(parts.track)),
      startupSummary_(std::move(parts.startupSummary)) {}

Result<void> Application::OpenScene(const std::optional<std::string>& track) {
    const auto toScene = [](auto scene) -> std::unique_ptr<Scene> { return scene; };
    Result<std::unique_ptr<Scene>> scene = [&] {
        if (!track) {
            return TestScene::Create(renderer_).transform(toScene);
        }
        const Nanoseconds started = platform::Now();
        assets::LoaderPool pool(assets::LoaderPool::DefaultWorkerCount());
        assets::TrackLoader loader(vfs_, pool);
        return TrackViewerScene::Create(renderer_, vfs_, loader, *track, started)
            .transform(toScene);
    }();
    if (!scene) {
        return std::unexpected(std::move(scene.error()));
    }
    if (frameLoop_.Scenes().Top() != nullptr) {
        frameLoop_.Scenes().Pop();
    }
    frameLoop_.Scenes().Push(std::move(*scene));
    return {};
}

void Application::CyclePacing() {
    StorePacing(settings_, NextPacingPreset(PacingFromSettings(settings_)));
    const std::array<std::string_view, 2> keys{kVsyncSetting.key, kFpsCapSetting.key};
    PersistSettings(settingsFile_, settings_, keys);
}

void Application::Persist(const std::vector<std::string>& keys) {
    if (!keys.empty()) {
        const std::vector<std::string_view> views(keys.begin(), keys.end());
        PersistSettings(settingsFile_, settings_, views);
    }
}

void Application::ApplyVideo() {
    renderer_.SetResolutionScale(static_cast<float>(settings_.Get(kResolutionScaleSetting)));
    const platform::PacingConfig pacing = PacingFromSettings(settings_);
    if (IsSamePacing(pacing, pacing_)) {
        return;
    }
    Result<void> changed = renderer_.SetPresentMode(pacing.mode);
    if (!changed) {
        log::Error("{}", changed.error().message);
        return;
    }
    pacer_.SetConfig(pacing);
    pacing_ = pacing;
    LogPacing(pacing);
}

}
