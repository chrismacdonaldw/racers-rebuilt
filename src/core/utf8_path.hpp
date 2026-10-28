#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace rr {

/// Never goes through the ANSI code page, unlike constructing a path from `char`.
[[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view utf8);

[[nodiscard]] std::string Utf8FromPath(const std::filesystem::path& path);

}
