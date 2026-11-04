#pragma once

#include <filesystem>
#include <vector>

#include "settings/quick_configs.hpp"
#include "settings/registry.hpp"
#include "settings/setting_preset.hpp"

namespace rr::app {

[[nodiscard]] std::vector<settings::SettingPreset> EnhancementPresets();

/// The shipped presets plus the user's from `<dataFolder>/presets/`.
[[nodiscard]] settings::QuickConfigs LoadQuickConfigs(const settings::Registry& registry,
                                                      const std::filesystem::path& dataFolder);

}
