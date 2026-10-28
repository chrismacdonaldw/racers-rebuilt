#pragma once

#include <filesystem>
#include <string_view>

#include "core/result.hpp"

namespace rr::platform {

/// File that, placed beside the exe, makes the exe's folder the data folder.
inline constexpr std::string_view kPortableMarker = "portable.txt";

/// Returns the folder that holds settings, logs, music and mods.
///
/// The exe's folder when it contains `portable.txt`, otherwise `%APPDATA%\racers-rebuilt\`,
/// which is created if missing.
[[nodiscard]] Result<std::filesystem::path> ResolveDataFolder();

/// Returns true when `exeFolder` contains `portable.txt`.
[[nodiscard]] bool IsPortable(const std::filesystem::path& exeFolder);

}
