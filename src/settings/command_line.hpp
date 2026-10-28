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
    std::optional<std::string> imageDumpFolder;
    std::vector<SettingOverride> overrides;
};

/// Accepts `--game-path <folder>`, `--set key=value` and `--dump-images <folder>`, each also as
/// `--flag=value`. `args` excludes the program name.
[[nodiscard]] Result<CommandLine> ParseCommandLine(std::span<const std::string_view> args);

[[nodiscard]] Result<void> ApplyOverrides(Registry& registry,
                                          std::span<const SettingOverride> overrides);

}
