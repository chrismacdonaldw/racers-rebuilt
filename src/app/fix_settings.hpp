#pragma once

#include <string_view>

#include "app/racer/champion_car.hpp"
#include "app/ui_text/drawn_glyphs.hpp"
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
    .label = "Draw a dash or slash a font has no glyph for",
    .category = kFixesCategory};

inline void DeclareFixSettings(settings::Registry& registry) {
    registry.Declare(kChampionCars);
    registry.Declare(kMissingGlyphs);
}

[[nodiscard]] inline CarBody CarBodyFrom(const settings::Registry& registry) {
    return registry.Get(kChampionCars) ? CarBody::Champion : CarBody::Bricks;
}

[[nodiscard]] inline MissingGlyphs MissingGlyphsFrom(const settings::Registry& registry) {
    return registry.Get(kMissingGlyphs) ? MissingGlyphs::Shapes : MissingGlyphs::FirstGlyph;
}

}
