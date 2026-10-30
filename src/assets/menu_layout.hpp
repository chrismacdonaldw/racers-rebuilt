#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "core/result.hpp"

namespace rr::assets {

inline constexpr std::int32_t kRootElementId = 1;

struct MenuRect {
    std::int32_t left = 0;
    std::int32_t top = 0;
    std::int32_t right = 0;
    std::int32_t bottom = 0;

    friend constexpr bool operator==(const MenuRect&, const MenuRect&) = default;
};

struct MenuColor {
    std::int32_t alpha = 0xFF;
    std::int32_t red = 0xFF;
    std::int32_t green = 0xFF;
    std::int32_t blue = 0xFF;

    friend constexpr bool operator==(const MenuColor&, const MenuColor&) = default;
};

struct MenuWidget {
    MenuRect rect;
    std::optional<std::int32_t> id;
    std::string parent;
    std::optional<std::int32_t> attachesToParent;
    std::optional<MenuColor> color;
};

inline constexpr std::size_t kControlStates = 6;

using StateImages = std::array<std::string, kControlStates>;
using StateFonts = std::array<std::string, kControlStates>;
using SoundIds = std::array<std::int32_t, 5>;
using CameraValues = std::array<float, 9>;

struct MenuControl {
    std::int32_t startsEnabled = 1;
    std::int32_t attachesToParent = 1;
    std::string name;
    std::optional<std::int32_t> value34;
    std::optional<std::int32_t> helpStringId;
    std::array<MenuColor, kControlStates> stateColors{};
    std::optional<SoundIds> soundIds;
};

struct RegionElement {};

struct ImageElement {
    std::string image;
    std::optional<MenuColor> color;
};

struct TextLabelElement {
    std::optional<std::string> font;
    std::optional<MenuColor> fontColor;
    std::optional<MenuColor> color;
    std::optional<std::int32_t> wrapWidth;
};

struct FrameElement {
    std::array<std::string, 8> images;
    std::optional<MenuColor> color;
    std::optional<MenuColor> fillColor;
    std::int32_t hasFillColor = 1;
};

struct ButtonElement {
    StateImages images;
};

struct MultiStateElement {
    std::vector<StateImages> imageSets;
    std::optional<StateFonts> fonts;
};

struct NestedButton {
    MenuWidget widget;
    MenuControl control;
    ButtonElement button;
};

struct HotspotElement {
    std::string highlightImage;
    std::optional<NestedButton> button;
};

struct SelectorElement {
    std::string previousButton;
    std::string nextButton;
    std::string frame;
    std::optional<std::int32_t> takesUnfocusedInput;
};

struct ModelCarouselElement {
    std::int32_t slotCount = 0;
    std::int32_t focusedSlot = 0;
    float scrollStep = 0.0F;
    std::vector<MenuRect> slots;
    std::optional<CameraValues> camera;
};

struct SliderElement {
    std::int32_t stepCount = 0;
    std::int32_t startValue = 0;
    std::string previousButton;
    std::string nextButton;
    std::string track;
    std::string thumb;
    std::optional<StateImages> images;
    std::optional<std::array<std::int32_t, 7>> soundIds;
};

struct SceneElement {
    std::string world;
    std::string linkedElement;
    std::optional<CameraValues> camera;
    std::optional<std::array<std::int32_t, 2>> worldFlags;
    std::optional<std::int32_t> clearMode;
};

struct TextFieldElement {
    std::string font;
    std::int32_t maxLength = 31;
    std::optional<std::array<std::int32_t, 9>> soundIds;
};

struct CutsceneElement {
    std::string cutscene;
    std::optional<std::array<std::int32_t, 2>> playback;
};

struct GameButtonElement {
    std::optional<StateFonts> fonts;
    std::optional<StateImages> images;
    std::optional<std::array<MenuColor, 13>> colors;
    std::optional<std::int32_t> maxTextWidth;
};

using MenuElementBody =
    std::variant<RegionElement, ImageElement, TextLabelElement, FrameElement, ButtonElement,
                 MultiStateElement, HotspotElement, SelectorElement, ModelCarouselElement,
                 SliderElement, SceneElement, TextFieldElement, CutsceneElement, GameButtonElement>;

struct MenuElement {
    std::string name;
    std::int32_t id = 0;
    std::int32_t parentId = kRootElementId;
    MenuWidget widget;
    std::optional<MenuControl> control;
    MenuElementBody body;
};

struct MenuLayout {
    std::int32_t capacity = 0;
    std::vector<MenuElement> elements;

    /// Names compare on their first 8 characters, as the original stores them.
    [[nodiscard]] const MenuElement* Find(std::string_view name) const;
};

[[nodiscard]] Result<MenuLayout> ParseMenuLayout(std::span<const std::byte> bytes);

[[nodiscard]] std::vector<std::string_view> ImageNames(const MenuElementBody& body);

}
