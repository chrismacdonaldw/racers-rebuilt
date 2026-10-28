#include "app/game_boot.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/game_settings.hpp"
#include "app/install_discovery.hpp"
#include "app/startup_settings.hpp"
#include "assets/folder_layer.hpp"
#include "assets/install.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "platform/clock.hpp"
#include "platform/data_folder.hpp"
#include "platform/install_locations.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kSettingsFileName = "settings.toml";
constexpr std::string_view kModsFolderName = "mods";

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

Result<assets::Vfs> MountGameFiles(GameInstall install, const mods::ModList& mods,
                                   const settings::Registry& registry) {
    Result<assets::FolderLayer> looseFiles =
        assets::FolderLayer::Scan(install.folder, assets::ScanDepth::TopLevelFiles);
    if (!looseFiles) {
        return std::unexpected(std::move(looseFiles.error()));
    }
    assets::Vfs vfs;
    vfs.Mount(std::make_unique<assets::FolderLayer>(std::move(*looseFiles)));
    vfs.Mount(std::make_unique<assets::JamArchive>(std::move(install.jam)));
    mods.MountEnabled(vfs, registry);
    log::Info("{} mods, {} enabled", mods.All().size(), mods.Enabled(registry).size());
    return vfs;
}

}

Result<GameBoot> BootGame(std::span<const std::string_view> args) {
    StartupTimer startup(platform::Now());
    Result<settings::CommandLine> commandLine = settings::ParseCommandLine(args);
    Result<std::filesystem::path> dataFolder =
        commandLine.and_then([](const auto&) { return platform::ResolveDataFolder(); });
    if (!dataFolder) {
        return std::unexpected(std::move(dataFolder.error()));
    }
    OpenLogFile(*dataFolder);
    log::Info("racers-rebuilt {} starting, data folder {}", RR_VERSION, Utf8FromPath(*dataFolder));
    std::filesystem::path settingsFile = *dataFolder / kSettingsFileName;
    Extensions extensions = LoadExtensions(*dataFolder / kModsFolderName);
    Result<settings::Registry> registry =
        LoadStartupSettings(settingsFile, commandLine->overrides, extensions);
    if (!registry) {
        return std::unexpected(std::move(registry.error()));
    }
    startup.EndPhase("settings", platform::Now());

    Result<platform::SdlContext> sdl = platform::SdlContext::Create();
    Result<assets::Vfs> vfs = sdl.and_then([&](const auto&) {
        return ResolveInstall(*commandLine, *registry, settingsFile)
            .and_then([&](GameInstall install) {
                return MountGameFiles(std::move(install), extensions.mods, *registry);
            });
    });
    if (!vfs) {
        return std::unexpected(std::move(vfs.error()));
    }
    startup.EndPhase("game files", platform::Now());

    return GameBoot{.startup = std::move(startup),
                    .commandLine = std::move(*commandLine),
                    .settings = std::move(*registry),
                    .settingsFile = std::move(settingsFile),
                    .extensions = std::move(extensions),
                    .sdl = std::move(*sdl),
                    .vfs = std::move(*vfs)};
}

}
