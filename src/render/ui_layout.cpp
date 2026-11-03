#include "render/ui_layout.hpp"

#include <algorithm>
#include <cmath>

namespace rr::render {

Viewport UiViewport(const Rect& rect) {
    return {.x = rect.x / kUiWidth,
            .y = rect.y / kUiHeight,
            .width = rect.width / kUiWidth,
            .height = rect.height / kUiHeight,
            .isOnUiCanvas = true};
}

PixelRect FitUiArea(std::uint32_t widthPx, std::uint32_t heightPx, UiFit fit) {
    if (fit == UiFit::Stretched) {
        return {.widthPx = widthPx, .heightPx = heightPx};
    }
    const float scale =
        std::min(static_cast<float>(widthPx) / kUiWidth, static_cast<float>(heightPx) / kUiHeight);
    const auto fitWidthPx =
        std::min(widthPx, static_cast<std::uint32_t>(std::lround(kUiWidth * scale)));
    const auto fitHeightPx =
        std::min(heightPx, static_cast<std::uint32_t>(std::lround(kUiHeight * scale)));
    return {.xPx = (widthPx - fitWidthPx) / 2,
            .yPx = (heightPx - fitHeightPx) / 2,
            .widthPx = fitWidthPx,
            .heightPx = fitHeightPx};
}

PixelRect ViewportArea(const Viewport& viewport, std::uint32_t widthPx, std::uint32_t heightPx,
                       UiFit uiFit, SceneFit sceneFit) {
    const bool isWithinCanvas = viewport.isOnUiCanvas || sceneFit == SceneFit::Canvas;
    const PixelRect within = isWithinCanvas ? FitUiArea(widthPx, heightPx, uiFit)
                                            : PixelRect{.widthPx = widthPx, .heightPx = heightPx};
    const auto edge = [](float fraction, std::uint32_t startPx, std::uint32_t sizePx) {
        const long px =
            static_cast<long>(startPx) + std::lround(fraction * static_cast<float>(sizePx));
        return static_cast<std::uint32_t>(std::max(px, 0L));
    };
    const std::uint32_t left = edge(viewport.x, within.xPx, within.widthPx);
    const std::uint32_t top = edge(viewport.y, within.yPx, within.heightPx);
    const std::uint32_t right = edge(viewport.x + viewport.width, within.xPx, within.widthPx);
    const std::uint32_t bottom = edge(viewport.y + viewport.height, within.yPx, within.heightPx);
    return {.xPx = left,
            .yPx = top,
            .widthPx = std::max(1U, right - std::min(left, right)),
            .heightPx = std::max(1U, bottom - std::min(top, bottom))};
}

float ViewAspect(const Viewport& viewport, const PixelRect& area, SceneFit sceneFit) {
    if (sceneFit == SceneFit::Canvas && !viewport.isOnUiCanvas) {
        return (kUiWidth * viewport.width) / (kUiHeight * viewport.height);
    }
    return static_cast<float>(area.widthPx) / static_cast<float>(area.heightPx);
}

UiToClip UiToClipTransform(const PixelRect& area, std::uint32_t widthPx, std::uint32_t heightPx) {
    const auto windowWidth = static_cast<float>(widthPx);
    const auto windowHeight = static_cast<float>(heightPx);
    return {.scaleX = 2.0F * static_cast<float>(area.widthPx) / (kUiWidth * windowWidth),
            .scaleY = -2.0F * static_cast<float>(area.heightPx) / (kUiHeight * windowHeight),
            .offsetX = (2.0F * static_cast<float>(area.xPx) / windowWidth) - 1.0F,
            .offsetY = 1.0F - (2.0F * static_cast<float>(area.yPx) / windowHeight)};
}

}
