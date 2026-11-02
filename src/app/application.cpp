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
#include "app/circuit_awards.hpp"
#include "app/circuit_leg.hpp"
#include "app/cutscene/cutscene_scene.hpp"
#include "app/extensions.hpp"
#include "app/game_settings.hpp"
#include "app/gameplay_fingerprint.hpp"
#include "app/input_bindings.hpp"
#include "app/menu/award_scene.hpp"
#include "app/menu/award_scene_name.hpp"
#include "app/menu/controls_scene.hpp"
#include "app/menu/language_scene.hpp"
#include "app/menu/main_menu_scene.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/menu/options_scene.hpp"
#include "app/menu/race_setup_scene.hpp"
#include "app/music_folder_source.hpp"
#include "app/pacing_presets.hpp"
#include "app/race_loading_screen.hpp"
#include "app/race_scene.hpp"
#include "app/race_scene_name.hpp"
#include "app/race_settings.hpp"
#include "app/replay_session.hpp"
#include "app/save_import.hpp"
#include "app/scene_music.hpp"
#include "app/screenshots.hpp"
#include "app/sprite_test_scene.hpp"
#include "app/startup_scene.hpp"
#include "app/startup_settings.hpp"
#include "app/test_scene.hpp"
#include "app/time_trial/trial_records.hpp"
#include "app/track_viewer_scene.hpp"
#include "app/video_settings.hpp"
#include "assets/loader_pool.hpp"
#include "assets/race_catalog.hpp"
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

template <typename T>
std::unique_ptr<Scene> ToScene(std::unique_ptr<T> scene) {
    return scene;
}

std::filesystem::path OriginalInstall(const GameBoot& boot) {
    return PathFromUtf8(
        boot.commandLine.gamePath.value_or(std::string(boot.settings.Get(kGamePath))));
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
    std::filesystem::path originalInstall = OriginalInstall(boot);

    return Application({.sdl = std::move(boot.sdl),
                        .settings = std::move(boot.settings),
                        .settingsFile = std::move(boot.settingsFile),
                        .originalInstall = std::move(originalInstall),
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
    Result<ReplaySession> replay = ReplaySession::Start(
        recordFile_, replayFile_, scene_, GameplayFingerprint(settings_, extensions_.mods));
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
        if (Scene* scene = frameLoop_.Scenes().Top(); scene != nullptr) {
            scene->HandleEvent(event);
        }
    });
    audio::MusicPlayer music =
        CreateMusicPlayer(mixer_, vfs_, settingsFile_.parent_path() / kMusicFolderName,
                          extensions_.mods.Enabled(settings_));
    AttachEnhancements(extensions_, frameLoop_.Hooks(), settings_);
    frameLoop_.Hooks().Subscribe<mods::SceneEntered>([this, &bindings](const mods::SceneEntered&) {
        bindings.SetPlayerCount(frameLoop_.Scenes().Top()->PlayerCount(), frameLoop_.Input());
    });
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
            bindings.Rebind(settings_, frameLoop_.Input());
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
    const std::vector<overlay::MenuChoice> scenes = SceneChoices();
    const std::vector<overlay::MenuChoice> saveImports = SaveImportChoices();
    Nanoseconds lastFrame = platform::Now();
    FrameStats frame;
    SceneMusicFollower sceneMusic;
    while (!events_.IsQuitRequested() && !isQuitting_) {
        events_.Poll();
        if (events_.WasKeyPressed(kToggleOverlayKey)) {
            menu.Toggle();
        }
        if (events_.WasKeyPressed(kCyclePacingKey)) {
            CyclePacing();
        }
        if (events_.WasKeyPressed(kScreenshotKey)) {
            renderer_.RequestCapture();
        }
        const Nanoseconds now = platform::Now();
        Scene* shown = frameLoop_.Scenes().Top();
        const bool showsScene = menu.IsVisible() && shown != nullptr;
        const overlay::RacerTools racerTools =
            showsScene ? shown->OverlayRacerTools() : overlay::RacerTools{};
        Persist(menu.Build(settings_,
                           {.frameTime = std::chrono::duration_cast<FloatSeconds>(now - lastFrame),
                            .ticksPerFrame = frame.tickCount,
                            .startupSummary = startupSummary_},
                           scenes, saveImports, racerTools));
        lastFrame = now;
        frame = frameLoop_.RunFrame(now);
        StoreSceneSettings();
        SaveCapturedFrame(renderer_, settingsFile_.parent_path());
        OpenNextScene();
        if (const Scene* top = frameLoop_.Scenes().Top(); top != nullptr) {
            sceneMusic.Update(music, top->Music(), frameLoop_.Scenes().ChangeCount());
        }
        pacer_.WaitForNextFrame();
    }
}

Application::Application(Parts parts)
    : sdl_(std::move(parts.sdl)),
      settings_(std::move(parts.settings)),
      settingsFile_(std::move(parts.settingsFile)),
      originalInstall_(std::move(parts.originalInstall)),
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

Result<std::unique_ptr<Scene>> Application::CreateScene(
    const std::optional<std::string>& requested) {
    const std::optional<std::string> scene =
        requested && *requested == kTrialEndScene ? std::optional(EndTimeTrial()) : requested;
    if (!scene || !IsTrialScene(*scene)) {
        trial_.reset();
    }
    if (!scene || *scene == kMainMenuScene) {
        return MainMenuScene::Create(renderer_, vfs_, LoadMenuSounds(mixer_, soundBanks_, vfs_),
                                     std::exchange(unlockNotice_, false))
            .transform(ToScene<MainMenuScene>);
    }
    if (*scene == kOptionsScene) {
        return OptionsScene::Create(renderer_, vfs_, LoadMenuSounds(mixer_, soundBanks_, vfs_))
            .transform(ToScene<OptionsScene>);
    }
    if (*scene == kControlsScene || *scene == kPlayer2ControlsScene) {
        const std::size_t player = *scene == kPlayer2ControlsScene ? 1 : 0;
        return ControlsScene::Create(renderer_, vfs_, player).transform(ToScene<ControlsScene>);
    }
    if (*scene == kLanguageScene) {
        return LanguageScene::Create(renderer_, vfs_, LoadMenuSounds(mixer_, soundBanks_, vfs_))
            .transform(ToScene<LanguageScene>);
    }
    if (const std::optional<RaceSetupMode> mode = RaceSetupModeOf(*scene)) {
        return RaceSetupScene::Create(renderer_, vfs_, LoadMenuSounds(mixer_, soundBanks_, vfs_),
                                      *mode, ReadProgress(ProgressFile()))
            .transform(ToScene<RaceSetupScene>);
    }
    if (*scene == kRecordsUnlockScene) {
        return AwardScene::CreateRecordsUnlock(renderer_, vfs_).transform(ToScene<AwardScene>);
    }
    if (IsAwardScene(*scene)) {
        return ParseAwardScene(*scene)
            .and_then([&](const AwardSceneSetup& award) {
                const Result<bool> opened = SaveAward(ProgressFile(), vfs_, award);
                if (!opened) {
                    log::Warn("{}, progress wasn't saved", opened.error().message);
                }
                unlockNotice_ = opened.value_or(false);
                return AwardScene::Create(renderer_, vfs_, award.award);
            })
            .transform(ToScene<AwardScene>);
    }
    return CreateWorldScene(*scene);
}

Result<std::unique_ptr<Scene>> Application::CreateWorldScene(std::string_view scene) {
    const Nanoseconds started = platform::Now();
    assets::LoaderPool pool(assets::LoaderPool::DefaultWorkerCount());
    assets::TrackLoader loader(vfs_, pool);
    if (IsRaceScene(scene)) {
        return ParseRaceScene(scene)
            .and_then([&](const RaceSceneSetup& setup) { return LoadRace(setup, loader, started); })
            .transform(ToScene<RaceScene>);
    }
    if (IsCircuitScene(scene)) {
        return CreateCircuitRace(scene, loader, started);
    }
    if (IsTrialScene(scene)) {
        return CreateTrialRace(scene, loader, started);
    }
    if (IsCutsceneScene(scene)) {
        return CutsceneScene::Create(
                   {.renderer = &renderer_, .mixer = &mixer_, .banks = &soundBanks_}, vfs_, loader,
                   SelectedLanguage(settings_), scene)
            .transform(ToScene<CutsceneScene>);
    }
    return TrackViewerScene::Create(renderer_, vfs_, loader, scene, started)
        .transform(ToScene<TrackViewerScene>);
}

Result<std::unique_ptr<Scene>> Application::CreateCircuitRace(std::string_view scene,
                                                              assets::TrackLoader& loader,
                                                              Nanoseconds started) {
    Result<CircuitLeg> leg = ParseCircuitScene(scene).and_then([&](const CircuitSceneSetup& setup) {
        return assets::RaceCatalog::Load(vfs_).and_then(
            [&](const assets::RaceCatalog& catalog) { return CircuitLeg::Create(catalog, setup); });
    });
    Result<std::unique_ptr<RaceScene>> race = leg.and_then(
        [&](const CircuitLeg& joined) { return LoadRace(joined.Race(), loader, started); });
    if (!race) {
        return std::unexpected(std::move(race.error()));
    }
    (*race)->JoinCircuit(std::move(*leg));
    return ToScene<RaceScene>(std::move(*race));
}

Result<std::unique_ptr<Scene>> Application::CreateTrialRace(std::string_view scene,
                                                            assets::TrackLoader& loader,
                                                            Nanoseconds started) {
    return ParseTrialScene(scene)
        .and_then([&](RaceSceneSetup setup) {
            if (!trial_ || trial_->Track() != setup.track) {
                trial_ = TimeTrialRun::Load(vfs_, setup.track);
            }
            setup.firstSlot = trial_->StartSlot();
            return LoadRace(setup, loader, started);
        })
        .transform([&](std::unique_ptr<RaceScene> race) {
            race->JoinTimeTrial(*trial_, loader);
            return ToScene<RaceScene>(std::move(race));
        });
}

std::string Application::EndTimeTrial() {
    const Result<bool> unlocked =
        trial_ ? SaveTrial(ProgressFile(), vfs_, trial_->Outcome()) : Result<bool>(false);
    if (!unlocked) {
        log::Warn("{}, the time trial wasn't saved", unlocked.error().message);
    }
    trial_.reset();
    return std::string(unlocked.value_or(false) ? kRecordsUnlockScene : kMainMenuScene);
}

Result<std::unique_ptr<RaceScene>> Application::LoadRace(const RaceSceneSetup& setup,
                                                         assets::TrackLoader& loader,
                                                         Nanoseconds started) {
    const std::string language = SelectedLanguage(settings_);
    const std::unique_ptr<RaceLoadingScreen> loading =
        OpenLoadingScreen(renderer_, vfs_, setup.track, language);
    Result<std::unique_ptr<RaceScene>> race =
        RaceScene::Create({.renderer = &renderer_, .mixer = &mixer_, .banks = &soundBanks_}, vfs_,
                          loader, setup, seed_, started);
    if (race && loading) {
        loading->Show(1.0F);
        (*race)->AdoptHudFonts(language, std::move(*loading).TakeFonts());
    }
    return race;
}

std::filesystem::path Application::ProgressFile() const {
    return settingsFile_.parent_path() / kProgressFileName;
}

Result<std::unique_ptr<Scene>> Application::CreateRace(const std::string& track) {
    return CreateScene(RaceNamed(track, 1, settings_));
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

std::vector<overlay::MenuChoice> Application::SceneChoices() {
    const auto choice = [this](std::string name, SceneFactory create) {
        return overlay::MenuChoice{.name = std::move(name),
                                   .choose = [this, create = std::move(create)] {
                                       const Result<void> opened = SwitchScene(create);
                                       if (!opened) {
                                           log::Error("{}", opened.error().message);
                                       }
                                   }};
    };
    std::vector<overlay::MenuChoice> choices{
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

std::vector<overlay::MenuChoice> Application::SaveImportChoices() {
    std::vector<overlay::MenuChoice> choices;
    for (std::filesystem::path& file : FindOriginalSaves(originalInstall_)) {
        std::string name = Utf8FromPath(file.parent_path().lexically_relative(originalInstall_));
        choices.push_back({.name = std::move(name), .choose = [this, file = std::move(file)] {
                               const Result<bool> imported =
                                   ImportOriginalSave(file, ProgressFile(), vfs_);
                               if (!imported) {
                                   log::Error("{}", imported.error().message);
                               }
                           }});
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

void Application::StoreSceneSettings() {
    Scene* scene = frameLoop_.Scenes().Top();
    if (scene == nullptr) {
        return;
    }
    std::vector<std::string> keys;
    for (SettingChange& change : scene->TakeSettingChanges()) {
        const Result<void> stored = settings_.Assign(change.key, std::move(change.value));
        if (!stored) {
            log::Error("{}", stored.error().message);
            continue;
        }
        keys.push_back(std::move(change.key));
    }
    Persist(keys);
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
