#pragma once

#include <string_view>

#include "app/racer/champion_car.hpp"
#include "app/ui_text/text_fixes.hpp"
#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"

namespace rr::app {

inline constexpr std::string_view kFixesCategory = "Enhancements/Fixes";

inline constexpr settings::SettingSpec<bool> kChampionCars{
    .key = "fixes.champion_cars",
    .label = "Champion quick builds look like the champion's car",
    .category = kFixesCategory};

inline constexpr settings::SettingSpec<bool> kMissingGlyphs{
    .key = "fixes.missing_glyphs",
    .label = "Draw missing letters, dashes and slashes readably",
    .category = kFixesCategory};

inline constexpr settings::SettingSpec<bool> kMenuFontGlyphs{
    .key = "fixes.menu_font_glyphs",
    .label = "English menus draw & and - from their own glyphs",
    .category = kFixesCategory};

inline void DeclareFixSettings(settings::Registry& registry) {
    registry.Declare(kChampionCars);
    registry.Declare(kMissingGlyphs);
    registry.Declare(kMenuFontGlyphs);
}

[[nodiscard]] inline CarBody CarBodyFrom(const settings::Registry& registry) {
    return registry.Get(kChampionCars) ? CarBody::Champion : CarBody::Bricks;
}

[[nodiscard]] inline TextFixes TextFixesFrom(const settings::Registry& registry) {
    return {.missingGlyphs =
                registry.Get(kMissingGlyphs) ? MissingGlyphs::Shapes : MissingGlyphs::FirstGlyph,
            .fixesGlyphLists = registry.Get(kMenuFontGlyphs)};
}

}
