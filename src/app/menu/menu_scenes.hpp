#pragma once

#include <charconv>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

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
inline constexpr std::string_view kPickSecondRacerScene = "menu:pick_racer2";
inline constexpr std::string_view kGarageScene = "menu:garage";
inline constexpr std::string_view kEditDriverScene = "menu:edit_driver";
inline constexpr std::string_view kLicenceScene = "menu:licence";
inline constexpr std::string_view kEditCarScene = "menu:edit_car";
inline constexpr std::string_view kCarBuildScene = "menu:car_build";
inline constexpr std::string_view kNewDriverScene = "menu:new_driver";
inline constexpr std::string_view kNewLicenceScene = "menu:new_licence";
inline constexpr std::string_view kNewCarScene = "menu:new_car";

inline constexpr char kSceneChainMark = '>';
inline constexpr char kSceneRacerMark = ':';

[[nodiscard]] inline std::string RacerScene(std::string_view head, std::size_t racer) {
    return std::format("{}{}{}", head, kSceneRacerMark, racer);
}

/// The racer index that follows `head`, or none when `scene` isn't `head` with one.
[[nodiscard]] inline std::optional<std::size_t> SceneRacer(std::string_view scene,
                                                           std::string_view head) {
    if (!scene.starts_with(head) || scene.size() <= head.size() + 1 ||
        scene[head.size()] != kSceneRacerMark) {
        return std::nullopt;
    }
    const std::string_view digits = scene.substr(head.size() + 1);
    std::size_t racer = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), racer);
    if (error != std::errc{} || end != digits.data() + digits.size()) {
        return std::nullopt;
    }
    return racer;
}

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
