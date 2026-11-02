#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "core/result.hpp"
#include "settings/command_line.hpp"
#include "settings/registry.hpp"

namespace rr::app {

/// A single race on `track` with the race settings' rules.
[[nodiscard]] std::string RaceNamed(std::string track, std::size_t players,
                                    const settings::Registry& settings);

/// The scene `--show-car`, `--track`, `--race` or `--cutscene` asks for, else the legal splash.
[[nodiscard]] Result<std::optional<std::string>> StartupScene(
    const settings::CommandLine& commandLine, const settings::Registry& settings);

}
