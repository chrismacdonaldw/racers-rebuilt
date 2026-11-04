#pragma once

#include "settings/setting_spec.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kEnhancedBuilder{
    .key = "enhancements.enhanced_builder",
    .defaultValue = false,
    .label = "Enhanced car builder",
    .category = "Enhancements/Menus",
    .tooltip = "Pick pieces from a palette and drag them onto the car."};

}
