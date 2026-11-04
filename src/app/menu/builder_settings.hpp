#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "render/render_view.hpp"
#include "settings/setting_spec.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kEnhancedBuilder{
    .key = "enhancements.enhanced_builder",
    .defaultValue = false,
    .label = "Enhanced car builder",
    .category = "Enhancements/Menus",
    .tooltip =
        "Opens a full-window builder with a free camera, drag and drop, and room to lay bricks on "
        "the ground."};

inline constexpr settings::SettingSpec<std::string_view> kBuilderColour{
    .key = "enhancements.builder_colour", .defaultValue = "#3A6EA5"};

inline constexpr std::string_view kColourBackground = "colour";

/// A race track's folder, or `kColourBackground`.
inline constexpr settings::SettingSpec<std::string_view> kBuilderBackground{
    .key = "enhancements.builder_background", .defaultValue = kColourBackground};

inline constexpr settings::SettingSpec<bool> kExtraPiecesSetting{
    .key = "enhancements.extra_pieces",
    .defaultValue = false,
    .label = "Extra LDraw pieces",
    .category = "Enhancements/Menus",
    .changesGameplay = true,
    .tooltip =
        "Adds an LDraw tab of bricks, plates, tiles, slopes and wedges to the enhanced car "
        "builder. Their shapes change how a car handles."};

/// `#RRGGBB`, else nothing.
[[nodiscard]] std::optional<render::Color> ParseHexColour(std::string_view text);

[[nodiscard]] std::string FormatHexColour(const render::Color& colour);

}
