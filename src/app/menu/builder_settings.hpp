#pragma once

#include "settings/setting_spec.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kEnhancedBuilder{
    .key = "enhancements.enhanced_builder",
    .defaultValue = false,
    .label = "Enhanced car builder",
    .category = "Enhancements/Menus",
    .tooltip =
        "Pick pieces from a palette, drag them onto the car, and move, recolour or remove "
        "placed pieces with undo and redo."};

inline constexpr settings::SettingSpec<bool> kExtraPiecesSetting{
    .key = "enhancements.extra_pieces",
    .defaultValue = false,
    .label = "Extra LDraw pieces",
    .category = "Enhancements/Menus",
    .changesGameplay = true,
    .tooltip =
        "Adds an LDraw tab of bricks, plates, tiles, slopes and wedges to the enhanced car "
        "builder. Their shapes change how a car handles."};

}
