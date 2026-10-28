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

#include "app/audio_startup.hpp"
#include "app/game_settings.hpp"
#include "app/install_discovery.hpp"
#include "app/startup_settings.hpp"
#include "app/startup_timer.hpp"
#include "app/test_scene.hpp"
#include "app/video_settings.hpp"
#include "assets/folder_layer.hpp"
#include "assets/install.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "platform/clock.hpp"
#include "platform/data_folder.hpp"
#include "platform/install_locations.hpp"

namespace rr::app {
namespace {

constexpr const char* kWindowTitle = "racers-rebuilt";
constexpr int kWindowWidthPx = 1280;
constexpr int kWindowHeightPx = 720;
constexpr std::string_view kSettingsFileName = "settings.toml";
constexpr SDL_Keycode kCyclePacingKey = SDLK_F9;

constexpr std::array kPacingPresets{
    platform::PacingConfig{.mode = platform::PresentMode::Vsync},
    platform::PacingConfig{.mode = platform::PresentMode::Capped, .fpsCap = 30},
    platform::PacingConfig{.mode = platform::PresentMode::Capped, .fpsCap = 60},
    platform::PacingConfig{.mode = platform::PresentMode::Capped, .fpsCap = 144},
    platform::PacingConfig{.mode = platform::PresentMode::Uncapped},
};

void OpenLogFile(const std::filesystem::path& dataFolder) {
    const std::filesystem::path logPath = dataFolder / "logs" / "racers-rebuilt.log";
    Result<void> opened = log::OpenFile(logPath);
    if (!opened) {
        log::Warn("{}, logging to stderr only", opened.error().message);
    }
}

std::optional<std::filesystem::path> OptionalPath(std::string_view utf8) {
    if (utf8.empty()) {
        return std::nullopt;
    }
    return PathFromUtf8(utf8);
}

std::string_view InstalledLanguage() {
    const std::optional<std::uint32_t> langId = platform::InstalledLanguageId();
    const std::optional<std::string_view> language =
        langId ? assets::LanguageForLangId(*langId) : std::nullopt;
    return language.value_or(assets::kDefaultLanguage);
}

Result<GameInstall> ResolveInstall(const settings::CommandLine& commandLine,
                                   settings::Registry& registry,
                                   const std::filesystem::path& settingsFile) {
    Result<GameInstall> install =
        DiscoverInstall({.commandLine = OptionalPath(commandLine.gamePath.value_or("")),
                         .setting = OptionalPath(registry.Get(kGamePath))});
    if (!install) {
        return install;
    }
    std::vector<std::string_view> changedKeys;
    if (install->wasPicked) {
        registry.Set(kGamePath, Utf8FromPath(install->folder));
        changedKeys.push_back(kGamePath.key);
    }
    if (registry.Get(kGameLanguage).empty()) {
        registry.Set(kGameLanguage, std::string(InstalledLanguage()));
        changedKeys.push_back(kGameLanguage.key);
    }
    if (!changedKeys.empty()) {
        PersistSettings(settingsFile, registry, changedKeys);
    }
    return install;
}

Result<assets::Vfs> MountGameFiles(GameInstall install) {
    Result<assets::FolderLayer> looseFiles =
        assets::FolderLayer::Scan(install.folder, assets::ScanDepth::TopLevelFiles);
    if (!looseFiles) {
        return std::unexpected(std::move(looseFiles.error()));
    }
    assets::Vfs vfs;
    vfs.Mount(std::make_unique<assets::FolderLayer>(std::move(*looseFiles)));
    vfs.Mount(std::make_unique<assets::JamArchive>(std::move(install.jam)));
    return vfs;
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

Result<platform::FramePacer> CreatePacer(const platform::PacingConfig& pacing) {
    Result<platform::FrameTimer> timer = platform::FrameTimer::Create();
    if (!timer) {
        return std::unexpected(std::move(timer.error()));
    }
    LogPacing(pacing);
    return platform::FramePacer(std::move(*timer), pacing);
}

}

Result<Application> Application::Create(std::span<const std::string_view> args) {
    StartupTimer startup(platform::Now());
    Result<settings::CommandLine> commandLine = settings::ParseCommandLine(args);
    Result<std::filesystem::path> dataFolder =
        commandLine.and_then([](const auto&) { return platform::ResolveDataFolder(); });
    if (!dataFolder) {
        return std::unexpected(std::move(dataFolder.error()));
    }
    OpenLogFile(*dataFolder);
    log::Info("racers-rebuilt {} starting, data folder {}", RR_VERSION, Utf8FromPath(*dataFolder));
    const std::filesystem::path settingsFile = *dataFolder / kSettingsFileName;
    Result<settings::Registry> registry = LoadStartupSettings(settingsFile, commandLine->overrides);
    if (!registry) {
        return std::unexpected(std::move(registry.error()));
    }
    startup.EndPhase("settings", platform::Now());

    Result<platform::SdlContext> sdl = platform::SdlContext::Create();
    Result<assets::Vfs> vfs = sdl.and_then([&](const auto&) {
        return ResolveInstall(*commandLine, *registry, settingsFile).and_then(MountGameFiles);
    });
    if (!vfs) {
        return std::unexpected(std::move(vfs.error()));
    }
    startup.EndPhase("game files", platform::Now());

    Result<platform::Window> window = platform::Window::Create(
        {.title = kWindowTitle, .widthPx = kWindowWidthPx, .heightPx = kWindowHeightPx});
    const platform::PacingConfig pacing = PacingFromSettings(*registry);
    Result<render::Renderer> renderer = window.and_then(
        [&](const platform::Window& w) { return render::Renderer::Create(w, pacing.mode); });
    Result<platform::FramePacer> pacer =
        renderer.and_then([&](const render::Renderer&) { return CreatePacer(pacing); });
    if (!pacer) {
        return std::unexpected(std::move(pacer.error()));
    }
    startup.EndPhase("window and renderer", platform::Now());

    Result<audio::Mixer> mixer = StartAudio(*registry, *vfs);
    if (!mixer) {
        return std::unexpected(std::move(mixer.error()));
    }
    startup.EndPhase("audio", platform::Now());
    log::Info("{}", startup.Summary());

    return Application({.sdl = std::move(*sdl),
                        .settings = std::move(*registry),
                        .settingsFile = settingsFile,
                        .vfs = std::move(*vfs),
                        .mixer = std::move(*mixer),
                        .window = std::move(*window),
                        .renderer = std::move(*renderer),
                        .pacer = std::move(*pacer)});
}

Result<void> Application::Run() {
    Result<std::unique_ptr<TestScene>> scene = TestScene::Create(renderer_);
    if (!scene) {
        return std::unexpected(std::move(scene.error()));
    }
    frameLoop_.Scenes().Push(std::move(*scene));
    settings_.Observe([this](std::string_view key) {
        if (key.starts_with("video.")) {
            ApplyPacing();
        }
    });

    while (!events_.IsQuitRequested()) {
        events_.Poll();
        if (events_.WasKeyPressed(kCyclePacingKey)) {
            CyclePacing();
        }
        frameLoop_.RunFrame(platform::Now());
        pacer_.WaitForNextFrame();
    }
    log::Info("quit requested, shutting down");
    return {};
}

Application::Application(Parts parts)
    : sdl_(std::move(parts.sdl)),
      settings_(std::move(parts.settings)),
      settingsFile_(std::move(parts.settingsFile)),
      vfs_(std::move(parts.vfs)),
      mixer_(std::move(parts.mixer)),
      window_(std::move(parts.window)),
      renderer_(std::move(parts.renderer)),
      pacer_(std::move(parts.pacer)) {}

void Application::CyclePacing() {
    StorePacing(settings_, NextPacingPreset(PacingFromSettings(settings_)));
    const std::array<std::string_view, 2> keys{kVsyncSetting.key, kFpsCapSetting.key};
    PersistSettings(settingsFile_, settings_, keys);
}

void Application::ApplyPacing() {
    const platform::PacingConfig pacing = PacingFromSettings(settings_);
    Result<void> changed = renderer_.SetPresentMode(pacing.mode);
    if (!changed) {
        log::Error("{}", changed.error().message);
        return;
    }
    pacer_.SetConfig(pacing);
    LogPacing(pacing);
}

}
