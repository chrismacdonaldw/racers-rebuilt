#include "app/menu/workshop/workshop_layout.hpp"

#include <algorithm>
#include <cmath>

namespace rr::app {
namespace {

constexpr float kMargin = 8.0F;
constexpr float kDrawerWidth = 196.0F;
constexpr float kTabGap = 6.0F;
constexpr float kTileGap = 6.0F;
constexpr std::size_t kTileColumns = 3;
constexpr float kLineHeight = 16.0F;
constexpr float kButtonHeight = 22.0F;
constexpr float kDoneWidth = 64.0F;
constexpr float kToolWidth = 52.0F;
constexpr float kToolGap = 4.0F;
constexpr float kSwatchStep = 28.0F;
constexpr std::size_t kSwatches = 10;

float Right(const render::Rect& area) {
    return area.x + area.width;
}

float Bottom(const render::Rect& area) {
    return area.y + area.height;
}

float Side(float width, std::size_t count, float gap) {
    return (width - (gap * static_cast<float>(count - 1))) / static_cast<float>(count);
}

PaletteGrid TilesBetween(float x, float top, float bottom, float width) {
    const float side = Side(width, kTileColumns, kTileGap);
    const auto rows = static_cast<std::size_t>(
        std::max(std::floor((bottom - top + kTileGap) / (side + kTileGap)), 1.0F));
    return {.area = {.x = x,
                     .y = top,
                     .width = width,
                     .height = (static_cast<float>(rows) * (side + kTileGap)) - kTileGap},
            .columns = kTileColumns,
            .rows = rows,
            .gap = kTileGap};
}

}

WorkshopLayout LayOutWorkshop(const render::Rect& window, std::size_t tabs) {
    WorkshopLayout layout;
    layout.drawer = {.x = window.x, .y = window.y, .width = kDrawerWidth, .height = window.height};
    layout.scene = {.x = Right(layout.drawer),
                    .y = window.y,
                    .width = std::max(window.width - kDrawerWidth, 0.0F),
                    .height = window.height};

    const float inner = kDrawerWidth - (2.0F * kMargin);
    const float tab = Side(inner, kTabColumns, kTabGap);
    const std::size_t tabRows =
        std::clamp((tabs + kTabColumns - 1) / kTabColumns, std::size_t{1}, kTabRows);
    layout.tabs = {.x = window.x + kMargin,
                   .y = window.y + kMargin,
                   .width = inner,
                   .height = (static_cast<float>(tabRows) * (tab + kTabGap)) - kTabGap};
    layout.counts = {.x = window.x + kMargin,
                     .y = Bottom(window) - kMargin - kLineHeight,
                     .width = inner,
                     .height = kLineHeight};
    layout.tiles = TilesBetween(window.x + kMargin, Bottom(layout.tabs) + kMargin,
                                layout.counts.y - kMargin, inner);

    const float top = window.y + kMargin;
    layout.done = {.x = Right(layout.scene) - kMargin - kDoneWidth,
                   .y = top,
                   .width = kDoneWidth,
                   .height = kButtonHeight};
    const float tools = (static_cast<float>(kWorkshopTools) * (kToolWidth + kToolGap)) - kToolGap;
    const float start = layout.scene.x + kMargin;
    const float room = layout.done.x - kMargin - start;
    const float firstTool = start + std::max((room - tools) / 2.0F, 0.0F);
    for (std::size_t tool = 0; tool < kWorkshopTools; ++tool) {
        layout.tools.at(tool) = {
            .x = firstTool + (static_cast<float>(tool) * (kToolWidth + kToolGap)),
            .y = top,
            .width = kToolWidth,
            .height = kButtonHeight};
    }

    const float swatches = kSwatchStep * static_cast<float>(kSwatches);
    layout.colours = {.x = layout.scene.x + ((layout.scene.width - swatches) / 2.0F),
                      .y = Bottom(window) - kMargin - kSwatchStep,
                      .width = swatches,
                      .height = kSwatchStep};
    layout.hint = {.x = layout.scene.x + kMargin,
                   .y = layout.colours.y - kMargin - kLineHeight,
                   .width = std::max(layout.scene.width - (2.0F * kMargin), 0.0F),
                   .height = kLineHeight};
    return layout;
}

render::Rect TabOf(const WorkshopLayout& layout, std::size_t tab) {
    const float side = Side(layout.tabs.width, kTabColumns, kTabGap);
    const std::size_t column = tab % kTabColumns;
    const std::size_t row = tab / kTabColumns;
    return {.x = layout.tabs.x + (static_cast<float>(column) * (side + kTabGap)),
            .y = layout.tabs.y + (static_cast<float>(row) * (side + kTabGap)),
            .width = side,
            .height = side};
}

bool Contains(const render::Rect& area, float x, float y) {
    return x >= area.x && x < Right(area) && y >= area.y && y < Bottom(area);
}

}
