#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "settings/registry.hpp"

#include <toml++/toml.hpp>

namespace rr::settings {

struct TomlFile {
    toml::table table;
    std::string leadingComments;
};

struct TomlSetting {
    std::string key;
    std::optional<SettingValue> value;
};

/// Nested tables join into dotted keys. A value of an unsupported type has no `value`.
[[nodiscard]] std::vector<TomlSetting> FlattenSettings(const toml::table& root);

/// Dotted keys become nested tables. A key that clashes with a table is logged and left out.
[[nodiscard]] toml::table NestSettings(const Registry::Values& values);

[[nodiscard]] Result<toml::table> ParseToml(std::string_view text, std::string_view sourceName);

/// A missing file reads as empty. `leadingComments` keeps the comment lines before the first key.
[[nodiscard]] Result<TomlFile> ReadTomlFile(const std::filesystem::path& path);

/// Writes through a temporary file, so a crash can't leave a partial file.
[[nodiscard]] Result<void> WriteTomlFile(const TomlFile& file, const std::filesystem::path& path);

/// Writes `keys` in, removing any `owned` key `keys` lacks. `header` heads a file without comments.
[[nodiscard]] Result<void> ReplaceTomlKeys(const std::filesystem::path& path, toml::table keys,
                                           std::span<const std::string_view> owned,
                                           std::string_view header);

}
