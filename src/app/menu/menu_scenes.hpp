#pragma once

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

/// Saves what the time trial set, then shows the main menu.
inline constexpr std::string_view kTrialEndScene = "menu:trial_end";

/// Stands in for `winvvcar`, shown when the last time trial record falls.
inline constexpr std::string_view kRecordsUnlockScene = "menu:records_unlock";

inline constexpr std::string_view kMenuTheme = "theme";

/// Asks the application to close rather than open a scene.
inline constexpr std::string_view kQuitScene = "quit";

}
