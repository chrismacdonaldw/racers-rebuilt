#pragma once

#include <string>
#include <string_view>

#include "core/result.hpp"

namespace rr::assets {

/// Uppercases one path component. Fails on `.`, `..`, empty names, and characters outside
/// printable ASCII or not allowed in Windows file names.
[[nodiscard]] Result<std::string> NormalizeGameName(std::string_view name);

/// Returns uppercase components joined by `/`, with no outer separators. Accepts `\` too. The
/// root folder is the empty string.
[[nodiscard]] Result<std::string> NormalizeGamePath(std::string_view path);

[[nodiscard]] std::string JoinGamePath(std::string_view folder, std::string_view name);

}
