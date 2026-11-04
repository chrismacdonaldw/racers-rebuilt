#include "app/menu/workshop/background_menu.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "app/menu/builder_settings.hpp"
#include "app/menu/tile_drawing.hpp"
#include "app/menu/workshop/workshop_layout.hpp"

namespace rr::app {
namespace {

constexpr float kPadding = 6.0F;
constexpr float kGap = 4.0F;
constexpr float kSwatchSide = 24.0F;
constexpr float kRowWidth = 120.0F;
constexpr float kRowHeight = 18.0F;
constexpr std::size_t kColumns = 2;
constexpr float kInner = (kRowWidth * static_cast<float>(kColumns)) + kGap;
constexpr render::Color kPanelColour{.r = 0.08F, .g = 0.09F, .b = 0.11F, .a = 0.92F};
constexpr render::Color kRowColour{.r = 0.17F, .g = 0.18F, .b = 0.21F};
constexpr render::Color kHoverColour{.r = 1.0F, .g = 1.0F, .b = 1.0F, .a = 0.6F};
constexpr render::Color kChosenColour{.r = 1.0F, .g = 0.87F, .b = 0.0F};

constexpr render::Color Rgb(std::uint32_t hex) {
    constexpr float kByte = 255.0F;
    return {.r = static_cast<float>((hex >> 16U) & 0xFFU) / kByte,
            .g = static_cast<float>((hex >> 8U) & 0xFFU) / kByte,
            .b = static_cast<float>(hex & 0xFFU) / kByte};
}

constexpr std::array<render::Color, kBackgroundColours> kColours{
    Rgb(0x3A6EA5), Rgb(0x1E2228), Rgb(0x6B7078), Rgb(0xD8DCE0),
    Rgb(0x87C4E8), Rgb(0x3E7A3A), Rgb(0xC8A86A), Rgb(0x6A3A78)};

}

BackgroundLayout LayOutBackgroundMenu(const render::Rect& button, const render::Rect& scene,
                                      std::size_t tracks) {
    const std::size_t rows = (tracks + kColumns - 1) / kColumns;
    const float trackHeight =
        rows == 0 ? 0.0F : (static_cast<float>(rows) * (kRowHeight + kGap)) - kGap;
    const float width = kInner + (2.0F * kPadding);
    const float right = scene.x + scene.width;
    BackgroundLayout layout;
    layout.panel = {
        .x = std::max(std::min(button.x + button.width - width, right - width), scene.x),
        .y = button.y + button.height + kGap,
        .width = width,
        .height = (2.0F * kPadding) + kSwatchSide + kGap + trackHeight};
    const float left = layout.panel.x + kPadding;
    const float top = layout.panel.y + kPadding;
    const float step = (kInner - kSwatchSide) / static_cast<float>(kBackgroundColours - 1);
    for (std::size_t colour = 0; colour < kBackgroundColours; ++colour) {
        layout.colours.at(colour) = {.x = left + (static_cast<float>(colour) * step),
                                     .y = top,
                                     .width = kSwatchSide,
                                     .height = kSwatchSide};
    }
    const float firstRow = top + kSwatchSide + kGap;
    for (std::size_t track = 0; track < tracks; ++track) {
        const std::size_t column = track % kColumns;
        const std::size_t row = track / kColumns;
        layout.tracks.push_back({.x = left + (static_cast<float>(column) * (kRowWidth + kGap)),
                                 .y = firstRow + (static_cast<float>(row) * (kRowHeight + kGap)),
                                 .width = kRowWidth,
                                 .height = kRowHeight});
    }
    return layout;
}

render::Color BackgroundColour(std::size_t index) {
    return kColours.at(index);
}

BackgroundMenu::BackgroundMenu(std::vector<BackdropTrack> tracks) : tracks_(std::move(tracks)) {}

void BackgroundMenu::Place(const render::Rect& button, const render::Rect& scene) {
    layout_ = LayOutBackgroundMenu(button, scene, tracks_.size());
    hovered_.reset();
}

bool BackgroundMenu::Offers(std::string_view folder) const {
    return std::ranges::any_of(tracks_,
                               [&](const BackdropTrack& track) { return track.folder == folder; });
}

bool BackgroundMenu::Covers(float x, float y) const {
    return isOpen_ && Contains(layout_.panel, x, y);
}

std::optional<BackgroundChoice> BackgroundMenu::Tick(const sim::MenuPointer& pointer) {
    hovered_.reset();
    if (!Covers(pointer.x, pointer.y)) {
        return std::nullopt;
    }
    std::optional<BackgroundChoice> choice;
    for (std::size_t colour = 0; colour < kBackgroundColours; ++colour) {
        if (Contains(layout_.colours.at(colour), pointer.x, pointer.y)) {
            hovered_ = layout_.colours.at(colour);
            choice = {.background = std::string(kColourBackground), .colour = kColours.at(colour)};
        }
    }
    for (std::size_t track = 0; track < tracks_.size(); ++track) {
        if (Contains(layout_.tracks.at(track), pointer.x, pointer.y)) {
            hovered_ = layout_.tracks.at(track);
            choice = {.background = tracks_[track].folder};
        }
    }
    if (!pointer.LeftPressed() || !choice) {
        return std::nullopt;
    }
    isOpen_ = false;
    return choice;
}

void BackgroundMenu::Render(const UiFont* font, std::string_view background,
                            const render::Color& colour,
                            std::vector<render::SpriteDraw>& sprites) const {
    if (!isOpen_) {
        return;
    }
    sprites.push_back({.bounds = layout_.panel, .tint = kPanelColour});
    const bool isColour = background == kColourBackground;
    const std::string chosenHex = FormatHexColour(colour);
    for (std::size_t index = 0; index < kBackgroundColours; ++index) {
        const render::Rect& swatch = layout_.colours.at(index);
        sprites.push_back({.bounds = swatch, .tint = kColours.at(index)});
        if (isColour && FormatHexColour(kColours.at(index)) == chosenHex) {
            DrawOutline(swatch, kChosenColour, sprites);
        }
    }
    for (std::size_t track = 0; track < tracks_.size(); ++track) {
        const render::Rect& row = layout_.tracks.at(track);
        sprites.push_back({.bounds = row, .tint = kRowColour});
        DrawFittedText(font, tracks_[track].name, row, {}, sprites);
        if (tracks_[track].folder == background) {
            DrawOutline(row, kChosenColour, sprites);
        }
    }
    if (hovered_) {
        DrawOutline(*hovered_, kHoverColour, sprites);
    }
}

}
