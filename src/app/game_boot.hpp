#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string_view>

#include "app/extensions.hpp"
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
};

/// `args` excludes the program name. `--racer` must name a racer in `racers.toml` or the game.
[[nodiscard]] Result<GameBoot> BootGame(std::span<const std::string_view> args);

}
