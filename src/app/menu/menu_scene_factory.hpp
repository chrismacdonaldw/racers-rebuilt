#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "app/race_scene_parts.hpp"
#include "app/scene.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "sim/race/cheats.hpp"

namespace rr::app {

struct MenuSceneParts {
    RaceOutputs outputs;
    const assets::Vfs* vfs = nullptr;
    std::filesystem::path progressFile;
    std::filesystem::path racersFile;
    std::string language;
    sim::Cheats* cheats = nullptr;
};

/// The menu screens other than the main menu. None when `scene` names none of them.
[[nodiscard]] std::optional<Result<std::unique_ptr<Scene>>> CreateMenuScene(
    const MenuSceneParts& parts, std::string_view scene);

}
