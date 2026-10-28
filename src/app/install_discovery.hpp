#pragma once

#include <filesystem>
#include <optional>
#include <span>

#include "assets/jam_archive.hpp"
#include "core/result.hpp"
#include "platform/install_locations.hpp"

namespace rr::app {

struct GameInstall {
    std::filesystem::path folder;
    assets::JamArchive jam;
    bool wasPicked = false;
};

struct InstallHints {
    std::optional<std::filesystem::path> commandLine;
    std::optional<std::filesystem::path> setting;
};

[[nodiscard]] std::optional<GameInstall> FirstValidInstall(
    std::span<const platform::InstallCandidate> candidates);

/// A `--game-path` that fails validation is an error. Otherwise tries `game.path`, the exe folder,
/// the registry and known folders, then asks the user until they pick a valid folder or cancel.
[[nodiscard]] Result<GameInstall> DiscoverInstall(const InstallHints& hints);

}
