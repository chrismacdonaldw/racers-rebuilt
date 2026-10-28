#include "app/application.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/game_settings.hpp"
#include "app/install_discovery.hpp"
#include "app/startup_settings.hpp"
#include "app/startup_timer.hpp"
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
constexpr float kFallbackRefreshRateHz = 60.0F;
constexpr std::string_view kSettingsFileName = "settings.toml";

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

Result<platform::FramePacer> CreatePacer(const platform::Window& window) {
    Result<platform::FrameTimer> timer = platform::FrameTimer::Create();
    if (!timer) {
        return std::unexpected(std::move(timer.error()));
    }
    const float refreshRateHz = window.RefreshRateHz().value_or(kFallbackRefreshRateHz);
    const platform::PacingConfig pacing{.mode = platform::PresentMode::Capped,
                                        .fpsCap = static_cast<int>(std::lround(refreshRateHz))};
    log::Info("pacing capped at {} FPS", pacing.fpsCap);
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
    Result<platform::FramePacer> pacer = window.and_then(CreatePacer);
    if (!pacer) {
        return std::unexpected(std::move(pacer.error()));
    }
    startup.EndPhase("window", platform::Now());
    log::Info("{}", startup.Summary());

    return Application({.sdl = std::move(*sdl),
                        .settings = std::move(*registry),
                        .vfs = std::move(*vfs),
                        .window = std::move(*window),
                        .pacer = std::move(*pacer)});
}

void Application::Run() {
    while (!events_.IsQuitRequested()) {
        events_.Poll();
        frameLoop_.RunFrame(platform::Now());
        pacer_.WaitForNextFrame();
    }
    log::Info("quit requested, shutting down");
}

Application::Application(Parts parts)
    : sdl_(std::move(parts.sdl)),
      settings_(std::move(parts.settings)),
      vfs_(std::move(parts.vfs)),
      window_(std::move(parts.window)),
      pacer_(std::move(parts.pacer)) {}

}
