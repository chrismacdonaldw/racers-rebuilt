#include "app/hud/powerup_panel.hpp"

#include <cstddef>
#include <optional>

#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kFirstRedItem = 2;
constexpr std::size_t kFirstYellowItem = 6;
constexpr std::size_t kHolder = 11;
constexpr std::size_t kHolderTint = 12;
constexpr std::size_t kNoLevelBadge = 12;
constexpr std::size_t kFirstBlueItem = 16;
constexpr std::size_t kFirstGreenItem = 20;
constexpr std::size_t kImageCount = 24;
constexpr float kLeft = render::kUiWidth / 32.0F;
constexpr float kBottomMargin = 10.0F;
constexpr float kItemCentreX = 31.0F / 64.0F;
constexpr float kItemCentreY = 38.0F / 64.0F;
constexpr float kLevelBadgeX = 24.0F / 64.0F;
constexpr render::Color kWhite{};

struct ColourLook {
    std::optional<std::size_t> firstItem;
    render::Color tint;
};

ColourLook LookOf(std::optional<assets::BrickColour> colour) {
    if (!colour) {
        return {.tint = kWhite};
    }
    switch (*colour) {
        case assets::BrickColour::Red:
            return {.firstItem = kFirstRedItem, .tint = {.r = 1.0F, .g = 0.0F, .b = 0.0F}};
        case assets::BrickColour::Yellow:
            return {.firstItem = kFirstYellowItem, .tint = {.r = 1.0F, .g = 1.0F, .b = 0.0F}};
        case assets::BrickColour::Blue:
            return {.firstItem = kFirstBlueItem,
                    .tint = {.r = 80.0F / 255.0F, .g = 80.0F / 255.0F, .b = 1.0F}};
        case assets::BrickColour::Green:
            return {.firstItem = kFirstGreenItem, .tint = {.r = 0.0F, .g = 1.0F, .b = 0.0F}};
    }
    return {.tint = kWhite};
}

float Width(const UiImage& image) {
    return static_cast<float>(image.widthPx);
}

float Height(const UiImage& image) {
    return static_cast<float>(image.heightPx);
}

}

void LayoutPowerupPanel(const sim::RacerSnapshot& racer, std::span<const UiImage> images,
                        std::vector<render::SpriteDraw>& out, float bottom) {
    const std::size_t level = racer.whiteCount;
    if ((!racer.heldColour && level == 0) || images.size() < kImageCount) {
        return;
    }
    const ColourLook look = LookOf(racer.heldColour);
    const UiImage& holder = images[kHolder];
    const render::Rect panel{.x = kLeft,
                             .y = bottom - (Height(holder) + kBottomMargin),
                             .width = Width(holder),
                             .height = Height(holder)};
    out.push_back({.texture = holder.texture, .bounds = panel, .tint = kWhite});
    out.push_back({.texture = images[kHolderTint].texture, .bounds = panel, .tint = look.tint});
    if (look.firstItem) {
        const UiImage& item = images[*look.firstItem + level];
        out.push_back(
            {.texture = item.texture,
             .bounds = {.x = panel.x + (panel.width * kItemCentreX) - (Width(item) / 2.0F),
                        .y = panel.y + (panel.height * kItemCentreY) - (Height(item) / 2.0F),
                        .width = Width(item),
                        .height = Height(item)},
             .tint = kWhite});
    }
    if (level > 0) {
        const UiImage& badge = images[kNoLevelBadge + level];
        out.push_back({.texture = badge.texture,
                       .bounds = {.x = panel.x + (panel.width * kLevelBadgeX),
                                  .y = panel.y,
                                  .width = Width(badge),
                                  .height = Height(badge)},
                       .tint = kWhite});
    }
}

}
