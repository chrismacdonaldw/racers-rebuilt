#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/menu_layout.hpp"
#include "core/result.hpp"

namespace rr::assets {

struct ImageStyle {
    std::string name;
    std::string image;
    std::optional<MenuColor> color;
};

struct GameButtonStyle {
    std::string name;
    GameButtonElement button;
    std::optional<std::array<MenuRect, 2>> rects2C;
    std::optional<std::int32_t> value2E;
};

struct MenuStyles {
    std::vector<ImageStyle> images;
    std::vector<GameButtonStyle> gameButtons;

    [[nodiscard]] const ImageStyle* FindImage(std::string_view name) const;
    [[nodiscard]] const GameButtonStyle* FindGameButton(std::string_view name) const;
};

/// Sections other than image and game button styles are skipped.
[[nodiscard]] Result<MenuStyles> ParseMenuStyles(std::span<const std::byte> bytes);

}
