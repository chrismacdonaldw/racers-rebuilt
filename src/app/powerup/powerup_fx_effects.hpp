#pragma once

#include <array>
#include <string_view>
#include <vector>

#include "render/fx_effect.hpp"
#include "sim/powerup/powerup_kind.hpp"

namespace rr::app {

inline constexpr std::string_view kTurboFlameEffect = "trbflam";
inline constexpr std::string_view kHoverFlameEffect = "trbhovr";
inline constexpr std::array<std::string_view, sim::kPowerupColourCount> kPickupEffects{
    "pickred", "pickblu", "pickgrn", "pickylw"};
inline constexpr std::string_view kWhitePickupEffect = "pickwht";
inline constexpr std::string_view kWarpSwirlEffect = "warpswl";
inline constexpr std::string_view kWarpOutEffect = "warpout";
inline constexpr std::string_view kWarpInEffect = "warpin";
inline constexpr std::string_view kMagnetPullEffect = "magpull";
inline constexpr std::string_view kMagnetPulseEffect = "magpuls";

/// Also replaces the original lightning strike and oil bubble emitters.
[[nodiscard]] std::vector<render::FxEffect> PowerupFxEffects();

}
