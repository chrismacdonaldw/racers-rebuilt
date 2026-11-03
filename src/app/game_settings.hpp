#pragma once

#include <string>
#include <string_view>

#include "assets/game_version.hpp"
#include "assets/install.hpp"
#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<std::string_view> kGamePath{
    .key = "game.path", .defaultValue = "", .label = "Game folder", .category = "Game"};

inline constexpr settings::SettingSpec<std::string_view> kGamePath2001{
    .key = "game.path_2001", .defaultValue = "", .label = "2001 game folder", .category = "Game"};

inline constexpr settings::SettingSpec<std::string_view> kGameVersion{
    .key = "game.version",
    .defaultValue = "1999",
    .label = "Game version",
    .category = "Game",
    .changesGameplay = true,
    .choices = assets::kGameVersionNames};

inline constexpr settings::SettingSpec<bool> kInputLag2001{
    .key = "game.input_lag_2001",
    .defaultValue = false,
    .label = "2001 input lag",
    .category = "Game",
    .changesGameplay = true,
    .tooltip =
        "The 2001 release reads the controls after each frame's step, so every frame drives "
        "on the controls from the frame before. Only in 2001 mode.",
    .requirement = settings::SettingRequirement{.key = kGameVersion.key, .choice = "2001"}};

[[nodiscard]] inline assets::GameVersion SelectedVersion(const settings::Registry& registry) {
    return assets::ParseGameVersion(registry.Get(kGameVersion))
        .value_or(assets::GameVersion::Release1999);
}

[[nodiscard]] inline const settings::SettingSpec<std::string_view>& GamePathFor(
    assets::GameVersion version) {
    return version == assets::GameVersion::Release2001 ? kGamePath2001 : kGamePath;
}

/// Empty until first launch fills it from the installer's `LangID`.
inline constexpr settings::SettingSpec<std::string_view> kGameLanguage{
    .key = "game.language", .defaultValue = "", .label = "Language", .category = "Game"};

/// `kGameLanguage`, or `ENGLISH` while it is empty.
[[nodiscard]] inline std::string SelectedLanguage(const settings::Registry& registry) {
    std::string language = registry.Get(kGameLanguage);
    if (language.empty()) {
        language = assets::kDefaultLanguage;
    }
    return language;
}

inline constexpr settings::SettingSpec<bool> kIntroVideosSetting{.key = "startup.intro_videos",
                                                                 .defaultValue = true,
                                                                 .label = "Intro videos",
                                                                 .category = "Game"};

inline void DeclareGameSettings(settings::Registry& registry) {
    registry.Declare(kGamePath);
    registry.Declare(kGamePath2001);
    registry.Declare(kGameVersion);
    registry.Declare(kInputLag2001);
    registry.Declare(kGameLanguage);
    registry.Declare(kIntroVideosSetting);
}

}
