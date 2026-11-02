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

struct TextStyle {
    std::string name;
    std::string font;
    std::optional<MenuColor> color;
    std::optional<std::int32_t> wrapWidth;
    std::int32_t centered = 0;
};

inline constexpr std::size_t kSelectorItemStates = 8;

struct IconStyle {
    std::optional<SoundIds> sounds;
    std::optional<std::array<MenuRect, kControlStates>> stateRects;
    std::optional<std::int32_t> transitionMs;
    std::optional<std::array<MenuColor, kControlStates>> stateColors;
};

struct GameButtonStyle {
    std::string name;
    IconStyle icon;
    GameButtonElement button;
    std::optional<std::array<MenuRect, 2>> rects2C;
    std::optional<std::int32_t> value2E;
};

/// Sound ids for a step forward, a step back, and a step off the end of a list that doesn't wrap.
using CarouselSounds = std::array<std::int32_t, 3>;

struct CarouselStyle {
    std::string name;
    CarouselSounds sounds{};
    std::optional<std::int32_t> scrollMs;
    std::int32_t pageMode = 0;
    std::int32_t wraps = 0;
};

struct SelectorStyle {
    std::string name;
    IconStyle icon;
    std::string frame;
    std::array<std::string, 2> arrows;
    std::optional<std::array<MenuColor, kSelectorItemStates>> itemColors;
};

struct MenuStyles {
    std::vector<ImageStyle> images;
    std::vector<TextStyle> texts;
    std::vector<CarouselStyle> carousels;
    std::vector<SelectorStyle> selectors;
    std::vector<GameButtonStyle> gameButtons;

    [[nodiscard]] const ImageStyle* FindImage(std::string_view name) const;
    [[nodiscard]] const TextStyle* FindText(std::string_view name) const;
    [[nodiscard]] const CarouselStyle* FindCarousel(std::string_view name) const;
    [[nodiscard]] const SelectorStyle* FindSelector(std::string_view name) const;
    [[nodiscard]] const GameButtonStyle* FindGameButton(std::string_view name) const;
};

/// Sections other than image, text, carousel, selector and game button styles are skipped.
[[nodiscard]] Result<MenuStyles> ParseMenuStyles(std::span<const std::byte> bytes);

}
