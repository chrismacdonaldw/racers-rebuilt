#pragma once

#include <filesystem>
#include <string>

#include "core/result.hpp"

#include <toml++/toml.hpp>

namespace rr::settings {

struct TomlFile {
    toml::table table;
    std::string leadingComments;
};

/// A missing file reads as empty. `leadingComments` keeps the comment lines before the first key.
[[nodiscard]] Result<TomlFile> ReadTomlFile(const std::filesystem::path& path);

/// Writes through a temporary file, so a crash can't leave a partial file.
[[nodiscard]] Result<void> WriteTomlFile(const TomlFile& file, const std::filesystem::path& path);

}
