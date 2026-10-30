#pragma once

#include <string_view>

namespace rr::app {

inline constexpr std::string_view kMainMenuScene = "menu:main";
inline constexpr std::string_view kSingleRaceScene = "menu:single_race";

/// Asks the application to close rather than open a scene.
inline constexpr std::string_view kQuitScene = "quit";

}
