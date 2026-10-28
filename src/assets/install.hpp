#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

#include "assets/jam_archive.hpp"
#include "core/result.hpp"

namespace rr::assets {

inline constexpr std::string_view kJamFileName = "LEGO.JAM";
inline constexpr std::string_view kDefaultLanguage = "ENGLISH";

/// Checks that `LEGO.JAM` exists and holds `GAMEDATA` and `MENUDATA`.
[[nodiscard]] Result<JamArchive> OpenInstall(const std::filesystem::path& folder);

[[nodiscard]] std::optional<std::string_view> LanguageForLangId(std::uint32_t langId);

}
