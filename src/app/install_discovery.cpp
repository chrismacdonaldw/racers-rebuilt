#include "app/install_discovery.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "assets/install.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "platform/data_folder.hpp"
#include "platform/folder_picker.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kPickerTitle = "Choose the LEGO Racers folder, the one with LEGO.JAM";
constexpr std::string_view kNoInstallMessage =
    "racers-rebuilt needs the files from your copy of LEGO Racers. Run it again and choose the "
    "folder that contains LEGO.JAM, or start it with --game-path <folder>.";

std::vector<platform::InstallCandidate> AutomaticCandidates(
    const std::optional<std::filesystem::path>& setting) {
    std::vector<platform::InstallCandidate> candidates;
    if (setting) {
        candidates.push_back({.folder = *setting, .source = "game.path setting"});
    }
    if (Result<std::filesystem::path> exeFolder = platform::ExeFolder()) {
        candidates.push_back({.folder = std::move(*exeFolder), .source = "exe folder"});
    } else {
        log::Warn("{}, not looking for the game beside the exe", exeFolder.error().message);
    }
    std::ranges::move(platform::RegistryInstallCandidates(), std::back_inserter(candidates));
    std::ranges::move(platform::KnownInstallCandidates(), std::back_inserter(candidates));
    return candidates;
}

Result<GameInstall> PickInstall() {
    std::string title(kPickerTitle);
    while (true) {
        Result<std::optional<std::filesystem::path>> picked = platform::PickFolder(title);
        if (!picked) {
            return std::unexpected(std::move(picked.error()));
        }
        if (!*picked) {
            return Fail(ErrorCode::NotFound, std::string(kNoInstallMessage));
        }
        Result<assets::JamArchive> jam = assets::OpenInstall(**picked);
        if (jam) {
            return GameInstall{
                .folder = std::move(**picked), .jam = std::move(*jam), .wasPicked = true};
        }
        log::Warn("picked folder rejected: {}", jam.error().message);
        title = std::format("{}. {}", jam.error().message, kPickerTitle);
    }
}

}

std::optional<GameInstall> FirstValidInstall(
    std::span<const platform::InstallCandidate> candidates) {
    for (const platform::InstallCandidate& candidate : candidates) {
        Result<assets::JamArchive> jam = assets::OpenInstall(candidate.folder);
        if (jam) {
            log::Info("game files found in {} ({})", Utf8FromPath(candidate.folder),
                      candidate.source);
            return GameInstall{.folder = candidate.folder, .jam = std::move(*jam)};
        }
        log::Info("skipping {}: {}", candidate.source, jam.error().message);
    }
    return std::nullopt;
}

Result<GameInstall> DiscoverInstall(const InstallHints& hints) {
    if (hints.commandLine) {
        Result<assets::JamArchive> jam = assets::OpenInstall(*hints.commandLine);
        if (!jam) {
            return Fail(jam.error().code, std::format("--game-path: {}", jam.error().message));
        }
        return GameInstall{.folder = *hints.commandLine, .jam = std::move(*jam)};
    }
    std::optional<GameInstall> found = FirstValidInstall(AutomaticCandidates(hints.setting));
    if (found) {
        return std::move(*found);
    }
    return PickInstall();
}

}
