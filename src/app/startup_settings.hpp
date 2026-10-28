#pragma once

#include <filesystem>
#include <span>
#include <string_view>

#include "app/extensions.hpp"
#include "core/result.hpp"
#include "settings/command_line.hpp"
#include "settings/registry.hpp"

namespace rr::app {

/// A malformed file falls back to defaults with a warning. A bad `--set` is an error.
[[nodiscard]] Result<settings::Registry> LoadStartupSettings(
    const std::filesystem::path& file, std::span<const settings::SettingOverride> overrides,
    const Extensions& extensions);

/// Writes only `keys`, so `--set` overrides stay session-only. Never overwrites a file it can't
/// read.
void PersistSettings(const std::filesystem::path& file, const settings::Registry& current,
                     std::span<const std::string_view> keys);

}
