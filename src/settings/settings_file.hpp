#pragma once

#include <filesystem>

#include "core/result.hpp"
#include "settings/registry.hpp"

namespace rr::settings {

/// A missing file loads nothing. Values that don't fit their setting are logged and skipped.
[[nodiscard]] Result<void> LoadSettings(Registry& registry, const std::filesystem::path& path);

/// Writes every value, unknown keys included, through a temporary file so a crash can't leave a
/// partial file.
[[nodiscard]] Result<void> SaveSettings(const Registry& registry,
                                        const std::filesystem::path& path);

}
