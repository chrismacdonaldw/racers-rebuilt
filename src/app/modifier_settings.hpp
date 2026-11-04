#pragma once

#include <array>
#include <string_view>

#include "app/race_modifiers.hpp"
#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"
#include "sim/powerup/powerup_kind.hpp"

namespace rr::app {

inline constexpr std::string_view kModifiersCategory = "Enhancements/Modifiers";
inline constexpr std::string_view kNoOnlyPowerup = "off";

inline constexpr std::array<std::string_view, sim::kPowerupKindCount + 1> kOnlyPowerupChoices{
    kNoOnlyPowerup, "cannonball", "grappling_hook", "lightning", "missile", "oil_slick",
    "dynamite",     "magnet",     "curse",          "shield",    "turbo",   "warp"};

inline constexpr settings::SettingSpec<int> kGravityModifier{
    .key = "modifiers.gravity",
    .defaultValue = 100,
    .range = settings::SettingRange<int>{.min = 25, .max = 100},
    .label = "Gravity (%)",
    .category = kModifiersCategory,
    .changesGameplay = true,
    .tooltip = "Under 100 makes every car float further off jumps."};

inline constexpr settings::SettingSpec<int> kSpeedModifier{
    .key = "modifiers.speed",
    .defaultValue = 100,
    .range = settings::SettingRange<int>{.min = 100, .max = 200},
    .label = "Every racer's top speed and acceleration (%)",
    .category = kModifiersCategory,
    .changesGameplay = true};

inline constexpr settings::SettingSpec<bool> kMirrorModifier{.key = "modifiers.mirror",
                                                             .label = "Mirror every track",
                                                             .category = kModifiersCategory,
                                                             .changesGameplay = true};

inline constexpr settings::SettingSpec<std::string_view> kOnlyPowerupModifier{
    .key = "modifiers.only_powerup",
    .defaultValue = kNoOnlyPowerup,
    .label = "Every power-up fires as",
    .category = kModifiersCategory,
    .changesGameplay = true,
    .choices = kOnlyPowerupChoices};

inline constexpr settings::SettingSpec<bool> kChaosBricksModifier{
    .key = "modifiers.chaos_bricks",
    .label = "Every brick picked up is a random colour and white count",
    .category = kModifiersCategory,
    .changesGameplay = true};

inline constexpr settings::SettingSpec<bool> kSqueakyVoicesModifier{
    .key = "modifiers.squeaky_voices",
    .label = "Drivers' voices and taunts an octave up",
    .category = kModifiersCategory};

inline constexpr settings::SettingSpec<bool> kRocketRushModifier{
    .key = "modifiers.rocket_rush",
    .label = "Every computer racer is Rocket Racer",
    .category = kModifiersCategory,
    .changesGameplay = true};

void DeclareModifierSettings(settings::Registry& registry);

[[nodiscard]] RaceModifiers ModifiersFrom(const settings::Registry& registry);

}
