#include "app/game_boot.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/circuit_awards.hpp"
#include "app/game_extraction.hpp"
#include "app/game_settings.hpp"
#include "app/install_discovery.hpp"
#include "app/racer/racer_lookup.hpp"
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
constexpr std::string_view kExtractedFolderName = "game";

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

GameInstall ExtractedOrInstall(GameInstall install, const std::filesystem::path& extractedFolder) {
    log::Info("copying the game files from {} to {}", Utf8FromPath(install.folder),
              Utf8FromPath(extractedFolder));
    Result<void> extracted = ExtractGameFiles(install.folder, extractedFolder);
    if (!extracted) {
        log::Warn("{}, reading the game files from {}", extracted.error().message,
                  Utf8FromPath(install.folder));
        return install;
    }
    std::optional<GameInstall> copy = OpenExtractedCopy(extractedFolder, std::nullopt);
    return copy ? std::move(*copy) : std::move(install);
}

Result<GameInstall> ResolveGameFiles(const settings::CommandLine& commandLine,
                                     settings::Registry& registry,
                                     const std::filesystem::path& settingsFile,
                                     const std::filesystem::path& dataFolder) {
    if (OptionalPath(commandLine.gamePath.value_or(""))) {
        return ResolveInstall(commandLine, registry, settingsFile);
    }
    const std::filesystem::path extractedFolder = dataFolder / kExtractedFolderName;
    std::optional<GameInstall> extracted =
        OpenExtractedCopy(extractedFolder, OptionalPath(registry.Get(kGamePath)));
    if (extracted) {
        return std::move(*extracted);
    }
    return ResolveInstall(commandLine, registry, settingsFile).transform([&](GameInstall install) {
        return ExtractedOrInstall(std::move(install), extractedFolder);
    });
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
        return ResolveGameFiles(*commandLine, *registry, settingsFile, *dataFolder)
            .and_then([&](GameInstall install) {
                return MountGameFiles(std::move(install), extensions.mods, *registry);
            });
    });
    if (!vfs) {
        return std::unexpected(std::move(vfs.error()));
    }
    std::optional<settings::Racer> playerRacer;
    if (commandLine->racer) {
        Result<settings::Racer> racer =
            FindRacer(*vfs, *dataFolder / kRacersFileName, *commandLine->racer);
        if (!racer) {
            return std::unexpected(std::move(racer.error()));
        }
        playerRacer = std::move(*racer);
    }
    startup.EndPhase("game files", platform::Now());

    return GameBoot{.startup = std::move(startup),
                    .commandLine = std::move(*commandLine),
                    .settings = std::move(*registry),
                    .settingsFile = std::move(settingsFile),
                    .extensions = std::move(extensions),
                    .sdl = std::move(*sdl),
                    .vfs = std::move(*vfs),
                    .playerRacer = std::move(playerRacer)};
}

}
