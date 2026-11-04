#pragma once

#include "app/fix_settings.hpp"
#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"
#include "sim/powerup/warp_rules.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kBetterWarp{
    .key = "race.better_warp",
    .label = "Better Warp: land facing a clear way along the track",
    .category = "Enhancements/Gameplay",
    .changesGameplay = true};

inline constexpr settings::SettingSpec<bool> kFairWarp{
    .key = "race.fair_warp",
    .label = "Fair Warp: the warp gains as much as a strong turbo",
    .category = "Enhancements/Gameplay",
    .changesGameplay = true,
    .tooltip = "For every racer, computer racers included."};

inline constexpr settings::SettingSpec<bool> kCatchUpWarp{
    .key = "race.catch_up_warp",
    .label = "Catch-up Warp: the race leader's warp gains a third as much",
    .category = "Enhancements/Gameplay",
    .changesGameplay = true,
    .tooltip = "For every racer, computer racers included."};

inline constexpr settings::SettingSpec<bool> kStuckWarpFix{
    .key = "fixes.stuck_warp",
    .label = "A warp taken back by a full pool ends properly",
    .category = kFixesCategory,
    .changesGameplay = true,
    .tooltip =
        "In the original, that racer stays a hidden, untouchable ghost for the rest of "
        "the race."};

inline void DeclareWarpSettings(settings::Registry& registry) {
    registry.Declare(kBetterWarp);
    registry.Declare(kFairWarp);
    registry.Declare(kCatchUpWarp);
    registry.Declare(kStuckWarpFix);
}

[[nodiscard]] inline sim::WarpRules WarpRulesFrom(const settings::Registry& registry) {
    return {.isFair = registry.Get(kFairWarp),
            .catchesUp = registry.Get(kCatchUpWarp),
            .aimsLanding = registry.Get(kBetterWarp),
            .endsWhenTakenBack = registry.Get(kStuckWarpFix)};
}

}
