#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "settings/registry.hpp"

namespace rr::settings {

struct SettingOverride {
    std::string key;
    std::string value;
};

struct CommandLine {
    std::optional<std::string> gamePath;
    std::vector<SettingOverride> overrides;
};

/// Accepts `--game-path <folder>` and `--set key=value`, each also as `--flag=value`. `args`
/// excludes the program name.
[[nodiscard]] Result<CommandLine> ParseCommandLine(std::span<const std::string_view> args);

[[nodiscard]] Result<void> ApplyOverrides(Registry& registry,
                                          std::span<const SettingOverride> overrides);

}
