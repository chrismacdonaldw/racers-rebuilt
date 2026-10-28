#include "render/ui_layout.hpp"

#include <algorithm>
#include <cmath>

namespace rr::render {

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

UiToClip UiToClipTransform(const PixelRect& area, std::uint32_t widthPx, std::uint32_t heightPx) {
    const auto windowWidth = static_cast<float>(widthPx);
    const auto windowHeight = static_cast<float>(heightPx);
    return {.scaleX = 2.0F * static_cast<float>(area.widthPx) / (kUiWidth * windowWidth),
            .scaleY = -2.0F * static_cast<float>(area.heightPx) / (kUiHeight * windowHeight),
            .offsetX = (2.0F * static_cast<float>(area.xPx) / windowWidth) - 1.0F,
            .offsetY = 1.0F - (2.0F * static_cast<float>(area.yPx) / windowHeight)};
}

}
