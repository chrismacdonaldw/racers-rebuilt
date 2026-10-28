#pragma once

#include <filesystem>
#include <string_view>

#include "core/result.hpp"

namespace rr::platform {

inline constexpr std::string_view kPortableMarker = "portable.txt";

/// The exe's folder when it contains `portable.txt`, otherwise `%APPDATA%\racers-rebuilt\`,
/// created if missing.
[[nodiscard]] Result<std::filesystem::path> ResolveDataFolder();

[[nodiscard]] Result<std::filesystem::path> ExeFolder();

[[nodiscard]] bool IsPortable(const std::filesystem::path& exeFolder);

}
