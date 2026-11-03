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
#include "app/cheat_settings.hpp"
#include "app/circuit_awards.hpp"
#include "app/circuit_leg.hpp"
#include "app/cutscene/cutscene_scene.hpp"
#include "app/cutscene/cutscene_screen.hpp"
#include "app/enhancement_presets.hpp"
#include "app/extensions.hpp"
#include "app/fix_settings.hpp"
#include "app/game_settings.hpp"
#include "app/gameplay_fingerprint.hpp"
#include "app/input_bindings.hpp"
#include "app/menu/award_scene_name.hpp"
#include "app/menu/main_menu_scene.hpp"
#include "app/menu/menu_scene_factory.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/music_folder_source.hpp"
#include "app/pacing_presets.hpp"
#include "app/race_loading_screen.hpp"
#include "app/race_scene.hpp"
#include "app/race_scene_name.hpp"
#include "app/race_settings.hpp"
#include "app/racer/racer_lookup.hpp"
#include "app/replay_session.hpp"
#include "app/save_import.hpp"
#include "app/scene_music.hpp"
#include "app/screenshots.hpp"
#include "app/sprite_test_scene.hpp"
#include "app/startup_scene.hpp"
#include "app/startup_settings.hpp"
#include "app/test_scene.hpp"
#include "app/time_trial/trial_records.hpp"
#include "app/video_settings.hpp"
#include "app/viewer_scene.hpp"
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
    Result<render::Renderer> renderer =
        render::Renderer::Create(*window, pacing.mode, render::RendererKind::Standard);
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
                        .startupSummary = std::move(startupSummary),
                        .playerRacer = std::move(boot.playerRacer),
                        .relaunch = std::move(boot.relaunch)});
}

Result<void> Application::Run() {
    Result<ReplaySession> replay =
        ReplaySession::Start(recordFile_, replayFile_, scene_, [this](std::string_view scene) {
            return GameplayFingerprint(
                settings_, extensions_.mods, extensions_.overrides.Hash(),
                {.first = FindPlayerRacer(vfs_, RacersFile(), ScenePlayer(scene)),
                 .second = FindPlayerRacer(vfs_, RacersFile(), SceneSecondPlayer(scene))});
        });
    if (!replay) {
        return std::unexpected(std::move(replay.error()));
    }
    seed_ = replay->Seed();
    MenuPointer pointer(window_.Handle(), renderer_, LoadMenuCursor(renderer_, vfs_));
    renderer_.PinTextures();
    AttachEnhancements(extensions_, frameLoop_.Hooks(), vfs_, settings_);
    Result<std::unique_ptr<overlay::Overlay>> menu =
        overlay::Overlay::Create(window_, renderer_, EnhancementPresets());
    Result<void> opened = menu.and_then([&](const auto&) { return OpenScene(scene_); });
    if (!opened) {
        return opened;
    }
    events_.Observe([&menu](const SDL_Event& event) { (*menu)->HandleEvent(event); });
    platform::Gamepads gamepads;
    InputBindings bindings(settings_);
    events_.Observe([this, &gamepads, &bindings, &pointer](const SDL_Event& event) {
        gamepads.Handle(event);
        bindings.Handle(event, frameLoop_.Input());
        pointer.Handle(event, frameLoop_.Input());
        if (Scene* scene = frameLoop_.Scenes().Top(); scene != nullptr) {
            scene->HandleEvent(event);
        }
    });
    audio::MusicPlayer music = CreateMusic();
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
    RunFrames(**menu, music, pointer);
    frameLoop_.SetTickTap(nullptr);
    replay->Finish();
    log::Info("quit requested, shutting down");
    return {};
}

void Application::RunFrames(overlay::Overlay& menu, audio::MusicPlayer& music,
                            MenuPointer& pointer) {
    std::vector<overlay::MenuChoice> scenes = SceneChoices();
    std::vector<overlay::MenuChoice> saveImports =
        SaveImportChoices(originalInstall_, ProgressFile(), vfs_);
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
                           scenes, saveImports, racerTools,
                           {.mods = ModActions(remount_, restart_, settings_, isQuitting_),
                            .game = GameActions(restart_, settings_, isQuitting_)}));
        lastFrame = now;
        if (remount_.TakeRequest()) {
            RemountMods(music);
            scenes = SceneChoices();
            saveImports = SaveImportChoices(originalInstall_, ProgressFile(), vfs_);
        }
        shown = frameLoop_.Scenes().Top();
        const bool pointsAtScene = !menu.IsVisible() && shown != nullptr;
        pointer.Show(pointsAtScene && shown->ShowsPointer(),
                     pointsAtScene && shown->HoldsPointer());
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
      startupSummary_(std::move(parts.startupSummary)),
      playerRacer_(std::move(parts.playerRacer)),
      restart_(std::move(parts.relaunch), settings_),
      remount_(!recordFile_ && !replayFile_, settings_) {}

audio::MusicPlayer Application::CreateMusic() {
    return CreateMusicPlayer(mixer_, vfs_, settingsFile_.parent_path() / kMusicFolderName,
                             extensions_.mods.Enabled(settings_));
}

void Application::RemountMods(audio::MusicPlayer& music) {
    music.Stop();
    const Result<void> opened = SwitchScene([this, &music] {
        trial_.reset();
        soundBanks_ = SoundBanks();
        RemountExtensions(extensions_, vfs_, settings_);
        music = CreateMusic();
        restart_.Rebase(settings_);
        remount_.Rebase(settings_);
        return CreateScene(std::string(kMainMenuScene));
    });
    if (!opened) {
        log::Error("{}", opened.error().message);
    }
}

Result<std::unique_ptr<Scene>> Application::CreateScene(
    const std::optional<std::string>& requested) {
    const std::optional<std::string> scene =
        requested && *requested == kTrialEndScene ? std::optional(EndTimeTrial()) : requested;
    if (!scene || !IsTrialScene(*scene)) {
        trial_.reset();
    }
    if (!scene || *scene == kMainMenuScene) {
        return MainMenuScene::Create(renderer_, vfs_, LoadMenuSounds(mixer_, soundBanks_, vfs_),
                                     SavedMenuChampion(ProgressFile()),
                                     std::exchange(unlockNotice_, false))
            .transform(ToScene<MainMenuScene>);
    }
    if (std::optional<Result<std::unique_ptr<Scene>>> menu = CreateMenuScene(
            {.outputs = {.renderer = &renderer_, .mixer = &mixer_, .banks = &soundBanks_},
             .vfs = &vfs_,
             .progressFile = ProgressFile(),
             .racersFile = RacersFile(),
             .language = SelectedLanguage(settings_),
             .cheats = &cheats_},
            *scene)) {
        return std::move(*menu);
    }
    if (IsAwardScene(*scene)) {
        return ParseAwardScene(*scene).and_then([&](const AwardSceneSetup& award) {
            const AwardClaim claim = ClaimAward(ProgressFile(), vfs_, award);
            unlockNotice_ = claim.opensCircuit;
            return CreateWorldScene(claim.screen);
        });
    }
    return CreateWorldScene(*scene);
}

Result<std::unique_ptr<Scene>> Application::CreateWorldScene(std::string_view scene) {
    const Nanoseconds started = platform::Now();
    assets::LoaderPool pool(assets::LoaderPool::DefaultWorkerCount());
    assets::TrackLoader loader(vfs_, pool);
    if (IsRaceScene(scene)) {
        return ParseRaceScene(scene)
            .and_then([&](RaceSceneSetup setup) {
                if (!setup.isDemo) {
                    setup.cheats = WithCheatToggles(RaceCheats(setup.cheats, cheats_), settings_);
                }
                return LoadRace(std::move(setup), loader, started);
            })
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
                   SelectedLanguage(settings_), scene,
                   {.playerRacer = AwardRacer(), .carBody = CarBodyFrom(settings_)})
            .transform(ToScene<CutsceneScene>);
    }
    return CreateViewerScene(renderer_, vfs_, loader, scene, started);
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
    return unlocked.value_or(false)
               ? CutsceneScreenName({.menu = std::string(kWinRecordsCarScreen)})
               : std::string(kMainMenuScene);
}

Result<std::unique_ptr<RaceScene>> Application::LoadRace(RaceSceneSetup setup,
                                                         assets::TrackLoader& loader,
                                                         Nanoseconds started) {
    if (Result<void> resolved = ResolvePlayers(vfs_, RacersFile(), setup); !resolved) {
        return std::unexpected(std::move(resolved.error()));
    }
    setup.isMirrored = LoadsMirrored(setup);
    setup.cheats = RaceCheats(setup.cheats, cheats_);
    setup.carBody = CarBodyFrom(settings_);
    setup.version = SelectedVersion(settings_);
    setup.lagsInput = settings_.Get(kInputLag2001);
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

std::filesystem::path Application::RacersFile() const {
    return settingsFile_.parent_path() / kRacersFileName;
}

std::optional<settings::Racer> Application::AwardRacer() const {
    return playerRacer_ ? playerRacer_ : SelectedRacerOrNone(RacersFile());
}

Result<std::unique_ptr<Scene>> Application::CreateRace(const std::string& track) {
    return CreateScene(RaceNamed({.track = track}, settings_));
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

}
