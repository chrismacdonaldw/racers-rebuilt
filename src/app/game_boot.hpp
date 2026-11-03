#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/extensions.hpp"
#include "app/install_discovery.hpp"
#include "app/startup_timer.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "platform/sdl_context.hpp"
#include "settings/command_line.hpp"
#include "settings/racers.hpp"
#include "settings/registry.hpp"

namespace rr::app {

// NOLINTNEXTLINE(bugprone-exception-escape)
struct GameBoot {
    StartupTimer startup;
    settings::CommandLine commandLine;
    settings::Registry settings;
    std::filesystem::path settingsFile;
    Extensions extensions;
    platform::SdlContext sdl;
    assets::Vfs vfs;
    std::optional<settings::Racer> playerRacer;
    std::vector<std::string> relaunch;
};

/// The player's own install from `--game-path` or the path setting, not the extracted copy.
[[nodiscard]] std::filesystem::path OriginalInstall(const GameBoot& boot);

/// Adopts the release `install` holds and returns the setting keys it changed.
[[nodiscard]] std::vector<std::string_view> RecordInstall(settings::Registry& registry,
                                                          const GameInstall& install,
                                                          bool isFromCommandLine);

/// `args` excludes the program name. `--racer` must name a racer in `racers.toml` or the game.
[[nodiscard]] Result<GameBoot> BootGame(std::span<const std::string_view> args);

}
