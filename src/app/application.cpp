#include "app/application.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/audio_settings.hpp"
#include "app/audio_startup.hpp"
#include "app/extensions.hpp"
#include "app/input_bindings.hpp"
#include "app/menu/main_menu_scene.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/menu/single_race_scene.hpp"
#include "app/music_folder_source.hpp"
#include "app/race_scene.hpp"
#include "app/race_settings.hpp"
#include "app/replay_session.hpp"
#include "app/sprite_test_scene.hpp"
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
#include "platform/clock.hpp"
#include "platform/gamepads.hpp"
#include "settings/command_line.hpp"

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

template <typename T>
std::unique_ptr<Scene> ToScene(std::unique_ptr<T> scene) {
    return scene;
}

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

std::string RaceNamed(std::string track, const settings::Registry& settings) {
    return RaceSceneName({.track = std::move(track), .respawn = RespawnRuleFrom(settings)});
}

Result<std::optional<std::string>> StartupScene(const settings::CommandLine& commandLine,
                                                const settings::Registry& settings) {
    if (commandLine.track && commandLine.race) {
        return Fail(ErrorCode::InvalidData, "--track and --race can't be used together");
    }
    return commandLine.race ? std::optional(RaceNamed(*commandLine.race, settings))
                            : commandLine.track;
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
    Result<std::optional<std::string>> scene = StartupScene(boot.commandLine, boot.settings);
    if (!scene) {
        return std::unexpected(std::move(scene.error()));
    }
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
    ApplyRendererSettings(boot.settings, *renderer);
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
                        .scene = std::move(*scene),
                        .recordFile = boot.commandLine.recordFile.transform(PathFromUtf8),
                        .replayFile = boot.commandLine.replayFile.transform(PathFromUtf8),
                        .startupSummary = std::move(startupSummary)});
}

Result<void> Application::Run() {
    Result<ReplaySession> replay = ReplaySession::Start(recordFile_, replayFile_, scene_);
    if (!replay) {
        return std::unexpected(std::move(replay.error()));
    }
    seed_ = replay->Seed();
    Result<std::unique_ptr<overlay::Overlay>> menu = overlay::Overlay::Create(window_, renderer_);
    Result<void> opened = menu.and_then([&](const auto&) { return OpenScene(scene_); });
    if (!opened) {
        return opened;
    }
    events_.Observe([&menu](const SDL_Event& event) { (*menu)->HandleEvent(event); });
    platform::Gamepads gamepads;
    InputBindings bindings(settings_);
    events_.Observe([this, &gamepads, &bindings](const SDL_Event& event) {
        gamepads.Handle(event);
        bindings.Handle(event, frameLoop_.Input());
    });
    audio::MusicPlayer music =
        CreateMusicPlayer(mixer_, vfs_, settingsFile_.parent_path() / kMusicFolderName,
                          extensions_.mods.Enabled(settings_));
    AttachEnhancements(extensions_, frameLoop_.Hooks(), settings_);
    settings_.Observe([this, &bindings](std::string_view key) {
        frameLoop_.Hooks().Emit(mods::SettingChanged{.key = key, .settings = settings_});
        if (Scene* scene = frameLoop_.Scenes().Top(); scene != nullptr) {
            scene->ApplySettings(settings_);
        }
        if (key.starts_with("video.")) {
            ApplyVideo();
        }
        if (key.starts_with("audio.")) {
            ApplyVolumes(settings_, mixer_);
        }
        if (key.starts_with("input.")) {
            frameLoop_.Input().ReleaseAll();
            bindings = InputBindings(settings_);
        }
    });

    frameLoop_.SetTickTap(replay->Tap());
    RunFrames(**menu, music);
    frameLoop_.SetTickTap(nullptr);
    replay->Finish();
    log::Info("quit requested, shutting down");
    return {};
}

void Application::RunFrames(overlay::Overlay& menu, audio::MusicPlayer& music) {
    const std::vector<overlay::SceneChoice> scenes = SceneChoices();
    Nanoseconds lastFrame = platform::Now();
    FrameStats frame;
    while (!events_.IsQuitRequested() && !isQuitting_) {
        events_.Poll();
        if (events_.WasKeyPressed(kToggleOverlayKey)) {
            menu.Toggle();
        }
        if (events_.WasKeyPressed(kCyclePacingKey)) {
            CyclePacing();
        }
        const Nanoseconds now = platform::Now();
        const Scene* scene = frameLoop_.Scenes().Top();
        Persist(menu.Build(settings_,
                           {.frameTime = std::chrono::duration_cast<FloatSeconds>(now - lastFrame),
                            .ticksPerFrame = frame.tickCount,
                            .startupSummary = startupSummary_,
                            .sceneStatus = scene != nullptr ? scene->DeveloperStatus() : ""},
                           scenes));
        lastFrame = now;
        frame = frameLoop_.RunFrame(now);
        OpenNextScene();
        if (frame.isNewScene && frameLoop_.Scenes().Top() != nullptr) {
            PlaySceneMusic(music, *frameLoop_.Scenes().Top());
        }
        pacer_.WaitForNextFrame();
    }
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
      scene_(std::move(parts.scene)),
      recordFile_(std::move(parts.recordFile)),
      replayFile_(std::move(parts.replayFile)),
      startupSummary_(std::move(parts.startupSummary)) {}

Result<std::unique_ptr<Scene>> Application::CreateScene(const std::optional<std::string>& scene) {
    if (!scene || *scene == kMainMenuScene) {
        return MainMenuScene::Create(renderer_, vfs_).transform(ToScene<MainMenuScene>);
    }
    if (*scene == kSingleRaceScene) {
        return SingleRaceScene::Create(renderer_, vfs_).transform(ToScene<SingleRaceScene>);
    }
    const Nanoseconds started = platform::Now();
    assets::LoaderPool pool(assets::LoaderPool::DefaultWorkerCount());
    assets::TrackLoader loader(vfs_, pool);
    if (IsRaceScene(*scene)) {
        return ParseRaceScene(*scene)
            .and_then([&](const RaceSceneSetup& setup) {
                return RaceScene::Create(renderer_, vfs_, loader, setup, seed_, started);
            })
            .transform(ToScene<RaceScene>);
    }
    return TrackViewerScene::Create(renderer_, vfs_, loader, *scene, started)
        .transform(ToScene<TrackViewerScene>);
}

Result<std::unique_ptr<Scene>> Application::CreateRace(const std::string& track) {
    return CreateScene(RaceNamed(track, settings_));
}

Result<void> Application::OpenScene(const std::optional<std::string>& scene) {
    return SwitchScene([&] { return CreateScene(scene); });
}

Result<void> Application::SwitchScene(const SceneFactory& create) {
    if (frameLoop_.Scenes().Top() != nullptr) {
        frameLoop_.Scenes().Pop();
        renderer_.ReleaseAssets();
    }
    Result<std::unique_ptr<Scene>> scene = create();
    if (!scene) {
        return std::unexpected(std::move(scene.error()));
    }
    (*scene)->ApplySettings(settings_);
    frameLoop_.Scenes().Push(std::move(*scene));
    return {};
}

std::vector<overlay::SceneChoice> Application::SceneChoices() {
    const auto choice = [this](std::string name, SceneFactory create) {
        return overlay::SceneChoice{.name = std::move(name),
                                    .open = [this, create = std::move(create)] {
                                        const Result<void> opened = SwitchScene(create);
                                        if (!opened) {
                                            log::Error("{}", opened.error().message);
                                        }
                                    }};
    };
    std::vector<overlay::SceneChoice> choices{
        choice("Test scene",
               [this] { return TestScene::Create(renderer_).transform(ToScene<TestScene>); }),
        choice("Sprite test",
               [this] {
                   return SpriteTestScene::Create(renderer_).transform(ToScene<SpriteTestScene>);
               }),
        choice("Main menu", [this] { return CreateScene(std::string(kMainMenuScene)); })};
    for (const std::string& track : assets::ListRaceTracks(vfs_)) {
        choices.push_back(
            choice(track, std::bind_front(&Application::CreateScene, this, std::optional(track))));
        choices.push_back(choice(std::format("{} race", track),
                                 std::bind_front(&Application::CreateRace, this, track)));
    }
    return choices;
}

void Application::OpenNextScene() {
    Scene* scene = frameLoop_.Scenes().Top();
    std::optional<std::string> next = scene != nullptr ? scene->TakeNextScene() : std::nullopt;
    if (!next) {
        return;
    }
    if (*next == kQuitScene) {
        isQuitting_ = true;
        return;
    }
    const Result<void> opened = OpenScene(next);
    if (!opened) {
        log::Error("{}", opened.error().message);
    }
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
    ApplyRendererSettings(settings_, renderer_);
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
