#pragma once

#include "app/cheat_settings.hpp"
#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"
#include "sim/race/player_cheats.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kInvincibleCheat{
    .key = "cheats.invincible",
    .label = "Weapons, hazards and curses can't touch you",
    .category = kCheatsCategory,
    .changesGameplay = true};

inline constexpr settings::SettingSpec<bool> kBottomlessBrickCheat{
    .key = "cheats.bottomless_brick",
    .label = "Using a power-up keeps it",
    .category = kCheatsCategory,
    .changesGameplay = true};

inline constexpr settings::SettingSpec<bool> kFullWhiteCheat{.key = "cheats.full_white",
                                                             .label = "Always hold 3 white bricks",
                                                             .category = kCheatsCategory,
                                                             .changesGameplay = true};

inline constexpr settings::SettingSpec<bool> kBrickMagnetCheat{
    .key = "cheats.brick_magnet",
    .label = "Pick up bricks from 3 times as far",
    .category = kCheatsCategory,
    .changesGameplay = true};

inline constexpr settings::SettingSpec<bool> kLongTurboCheat{
    .key = "cheats.long_turbo",
    .label = "Turbo boosts last 3 times as long",
    .category = kCheatsCategory,
    .changesGameplay = true};

inline constexpr settings::SettingSpec<bool> kPerfectStartCheat{
    .key = "cheats.perfect_start",
    .label = "Always get the best start boost",
    .category = kCheatsCategory,
    .changesGameplay = true};

inline constexpr settings::SettingSpec<bool> kFreeWarpCheat{.key = "cheats.free_warp",
                                                            .label = "Every green power-up warps",
                                                            .category = kCheatsCategory,
                                                            .changesGameplay = true};

inline constexpr settings::SettingSpec<int> kHeadStartCheat{
    .key = "cheats.head_start",
    .defaultValue = 0,
    .range = settings::SettingRange<int>{.min = 0, .max = 10},
    .label = "Head start over computer racers (s)",
    .category = kCheatsCategory,
    .changesGameplay = true};

void DeclarePlayerCheatSettings(settings::Registry& registry);

[[nodiscard]] sim::PlayerCheats PlayerCheatsFrom(const settings::Registry& registry);

}
