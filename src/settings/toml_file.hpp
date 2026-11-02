#pragma once

#include <filesystem>
#include <string_view>

#include "core/result.hpp"

#include <toml++/toml.hpp>

namespace rr::settings {

/// A missing file reads as an empty table.
[[nodiscard]] Result<toml::table> ReadTomlFile(const std::filesystem::path& path);

/// Writes `header` then `table` through a temporary file, so a crash can't leave a partial file.
[[nodiscard]] Result<void> WriteTomlFile(const toml::table& table,
                                         const std::filesystem::path& path,
                                         std::string_view header = {});

}
