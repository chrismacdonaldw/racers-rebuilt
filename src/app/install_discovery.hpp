#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>

#include "assets/game_version.hpp"
#include "assets/jam_archive.hpp"
#include "core/result.hpp"
#include "platform/install_locations.hpp"

namespace rr::app {

struct GameInstall {
    std::filesystem::path folder;
    assets::JamArchive jam;
    assets::GameVersion version = assets::GameVersion::Release1999;
    bool wasPicked = false;
};

struct InstallHints {
    std::optional<std::filesystem::path> commandLine;
    std::optional<std::filesystem::path> setting;
    std::optional<std::filesystem::path> otherSetting;
    assets::GameVersion version = assets::GameVersion::Release1999;
    bool isVersionForced = false;
};

/// `assets::OpenInstall`, failing on a folder that holds another release.
[[nodiscard]] Result<assets::JamArchive> OpenRelease(const std::filesystem::path& folder,
                                                     assets::GameVersion version);

[[nodiscard]] std::string RejectedFolderMessage(const Error& error);

/// Prefers an install of `version`, then the first install of another release.
[[nodiscard]] std::optional<GameInstall> FirstValidInstall(
    std::span<const platform::InstallCandidate> candidates, assets::GameVersion version);

/// Prefers `version` but takes another release, unless `isVersionForced` and a `--game-path` hold
/// it. Tries `setting`, the exe folder, the registry and known folders, then asks the user.
[[nodiscard]] Result<GameInstall> DiscoverInstall(const InstallHints& hints);

}
