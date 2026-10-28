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

/// A game path such as `GAMEDATA/COMMON/321.PCM` or `THEME.TUN`, played once audio starts.
inline constexpr settings::SettingSpec<std::string_view> kDevPlaySound{
    .key = "dev.play_sound", .defaultValue = "", .label = "Play sound", .category = "Developer"};

inline void DeclareGameSettings(settings::Registry& registry) {
    registry.Declare(kGamePath);
    registry.Declare(kGameLanguage);
    registry.Declare(kDevPlaySound);
}

}
