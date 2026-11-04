#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/result.hpp"
#include "settings/registry.hpp"
#include "settings/setting_preset.hpp"

namespace rr::settings {

struct QuickConfig {
    std::string name;
    std::vector<std::pair<std::string, SettingValue>> values;
};

[[nodiscard]] Result<QuickConfig> ReadQuickConfig(const std::filesystem::path& path);

[[nodiscard]] Result<void> WriteQuickConfig(const QuickConfig& config,
                                            const std::filesystem::path& path);

/// Lowercase letters, digits and dashes, or `config` when nothing is left.
[[nodiscard]] std::string QuickConfigSlug(std::string_view name);

/// Settings under a category `shipped` resets, named by `shipped`, or under Mods.
[[nodiscard]] std::vector<std::string> QuickConfigScope(const Registry& registry,
                                                        std::span<const SettingPreset> shipped);

[[nodiscard]] QuickConfig SnapshotQuickConfig(const Registry& registry,
                                              std::span<const std::string> scope, std::string name);

/// Every `scope` setting the config lacks takes its default. Keys outside `scope` are ignored.
[[nodiscard]] SettingPreset QuickConfigPreset(const QuickConfig& config, const Registry& registry,
                                              std::span<const std::string> scope, std::string menu);

}
