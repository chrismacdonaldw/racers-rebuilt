#pragma once

#include <optional>
#include <string>

#include "app/race_scene_name.hpp"
#include "core/result.hpp"
#include "settings/command_line.hpp"
#include "settings/registry.hpp"

namespace rr::app {

/// `setup` with the race settings' respawn and AI driving.
[[nodiscard]] std::string RaceNamed(RaceSceneSetup setup, const settings::Registry& settings);

/// The scene `--show-car`, `--track`, `--race` or `--cutscene` asks for, else the legal splash.
[[nodiscard]] Result<std::optional<std::string>> StartupScene(
    const settings::CommandLine& commandLine, const settings::Registry& settings);

}
