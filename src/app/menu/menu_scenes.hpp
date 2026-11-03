#pragma once

#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace rr::app {

inline constexpr std::string_view kMainMenuScene = "menu:main";
inline constexpr std::string_view kSingleRaceScene = "menu:single_race";
inline constexpr std::string_view kCircuitRaceScene = "menu:circuit_race";
inline constexpr std::string_view kVersusRaceScene = "menu:versus_race";
inline constexpr std::string_view kTimeTrialScene = "menu:time_trial";
inline constexpr std::string_view kOptionsScene = "menu:options";
inline constexpr std::string_view kControlsScene = "menu:controls";
inline constexpr std::string_view kPlayer2ControlsScene = "menu:controls2";
inline constexpr std::string_view kLanguageScene = "menu:language";
inline constexpr std::string_view kPickRacerScene = "menu:pick_racer";

inline constexpr char kSceneChainMark = '>';

[[nodiscard]] inline std::string ChainedScene(std::string_view head, std::string_view next) {
    return std::format("{}{}{}", head, kSceneChainMark, next);
}

/// What follows `head` in a chain it opens, or none when `scene` doesn't open with it.
[[nodiscard]] inline std::optional<std::string_view> SceneAfter(std::string_view scene,
                                                                std::string_view head) {
    if (!scene.starts_with(head) || scene.size() <= head.size() + 1 ||
        scene[head.size()] != kSceneChainMark) {
        return std::nullopt;
    }
    return scene.substr(head.size() + 1);
}

/// Saves what the time trial set, then shows the main menu.
inline constexpr std::string_view kTrialEndScene = "menu:trial_end";

inline constexpr std::string_view kMenuTheme = "theme";

/// Asks the application to close rather than open a scene.
inline constexpr std::string_view kQuitScene = "quit";

}
