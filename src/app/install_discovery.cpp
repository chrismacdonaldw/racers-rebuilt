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
#include "platform/message_box.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kPickerTitle = "Choose the LEGO Racers {}folder, the one with LEGO.JAM";
constexpr std::string_view kRightFolderHint =
    "Choose the folder LEGO Racers is installed in. It holds LEGO.JAM and LEGORacers.exe, and is "
    R"(usually Program Files (x86)\LEGO Media\Games\LEGO Racers.)";
constexpr std::string_view kNoInstallMessage =
    "racers-rebuilt needs the files from your {}copy of LEGO Racers. Run it again and choose the "
    "folder that contains LEGO.JAM, or start it with --game-path <folder>.";

std::string ReleaseLabel(assets::GameVersion version) {
    return version == assets::GameVersion::Release1999
               ? std::string()
               : std::format("{} ", assets::NameOf(version));
}

std::vector<platform::InstallCandidate> AutomaticCandidates(const InstallHints& hints) {
    std::vector<platform::InstallCandidate> candidates;
    if (hints.setting) {
        candidates.push_back({.folder = *hints.setting, .source = "game folder setting"});
    }
    if (hints.otherSetting) {
        candidates.push_back(
            {.folder = *hints.otherSetting, .source = "other release's game folder setting"});
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

std::unexpected<Error> FailWrongRelease(const std::filesystem::path& folder,
                                        assets::GameVersion held, assets::GameVersion wanted) {
    return Fail(ErrorCode::InvalidData,
                std::format("{} holds the {} release, not {}", Utf8FromPath(folder),
                            assets::NameOf(held), assets::NameOf(wanted)));
}

Result<GameInstall> OpenAnyRelease(const std::filesystem::path& folder) {
    return assets::OpenInstall(folder).transform([&](assets::JamArchive jam) {
        const assets::GameVersion version = assets::IdentifyRelease(jam);
        return GameInstall{.folder = folder, .jam = std::move(jam), .version = version};
    });
}

Result<GameInstall> PickInstall(assets::GameVersion version) {
    const std::string title = std::format(kPickerTitle, ReleaseLabel(version));
    while (true) {
        Result<std::optional<std::filesystem::path>> picked = platform::PickFolder(title);
        if (!picked) {
            return std::unexpected(std::move(picked.error()));
        }
        if (!*picked) {
            return Fail(ErrorCode::NotFound, std::format(kNoInstallMessage, ReleaseLabel(version)));
        }
        Result<GameInstall> install = OpenAnyRelease(**picked);
        if (install) {
            install->wasPicked = true;
            return install;
        }
        log::Warn("picked folder rejected: {}", install.error().message);
        platform::ShowErrorBox(RejectedFolderMessage(install.error()));
    }
}

}

Result<assets::JamArchive> OpenRelease(const std::filesystem::path& folder,
                                       assets::GameVersion version) {
    Result<assets::JamArchive> jam = assets::OpenInstall(folder);
    if (!jam) {
        return jam;
    }
    const assets::GameVersion held = assets::IdentifyRelease(*jam);
    if (held != version) {
        return FailWrongRelease(folder, held, version);
    }
    return jam;
}

std::string RejectedFolderMessage(const Error& error) {
    return std::format("That folder isn't a usable LEGO Racers install.\n\nProblem: {}\n\n{}",
                       error.message, kRightFolderHint);
}

std::optional<GameInstall> FirstValidInstall(std::span<const platform::InstallCandidate> candidates,
                                             assets::GameVersion version) {
    std::optional<GameInstall> otherRelease;
    for (const platform::InstallCandidate& candidate : candidates) {
        Result<GameInstall> install = OpenAnyRelease(candidate.folder);
        if (!install) {
            log::Info("skipping {}: {}", candidate.source, install.error().message);
            continue;
        }
        log::Info("{} game files found in {} ({})", assets::NameOf(install->version),
                  Utf8FromPath(candidate.folder), candidate.source);
        if (install->version == version) {
            return std::move(*install);
        }
        if (!otherRelease) {
            otherRelease = std::move(*install);
        }
    }
    return otherRelease;
}

Result<GameInstall> DiscoverInstall(const InstallHints& hints) {
    if (hints.commandLine) {
        Result<GameInstall> install = OpenAnyRelease(*hints.commandLine);
        if (install && hints.isVersionForced && install->version != hints.version) {
            install = FailWrongRelease(*hints.commandLine, install->version, hints.version);
        }
        if (!install) {
            return Fail(install.error().code,
                        std::format("--game-path: {}", install.error().message));
        }
        return install;
    }
    std::optional<GameInstall> found = FirstValidInstall(AutomaticCandidates(hints), hints.version);
    if (found) {
        return std::move(*found);
    }
    return PickInstall(hints.version);
}

}
