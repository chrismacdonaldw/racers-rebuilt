#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace rr {

/// Converts a UTF-8 string, as SDL and settings files use, to a path.
///
/// Unlike constructing a path from `char`, this never goes through the ANSI code page.
[[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view utf8);

/// Converts a path to UTF-8 for logs, messages and SDL calls. Never throws.
[[nodiscard]] std::string Utf8FromPath(const std::filesystem::path& path);

}
