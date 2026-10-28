#pragma once

#include <string_view>

#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<std::string_view> kGamePath{
    .key = "game.path", .defaultValue = "", .label = "Game folder", .category = "Game"};

/// Empty until first launch fills it from the installer's `LangID`.
inline constexpr settings::SettingSpec<std::string_view> kGameLanguage{
    .key = "game.language", .defaultValue = "", .label = "Language", .category = "Game"};

inline void DeclareGameSettings(settings::Registry& registry) {
    registry.Declare(kGamePath);
    registry.Declare(kGameLanguage);
}

}
