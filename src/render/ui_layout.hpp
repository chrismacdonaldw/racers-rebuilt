#pragma once

#include <cstdint>
#include <optional>

#include "render/render_view.hpp"

namespace rr::render {

/// UI coordinates span this 4:3 canvas, origin top left.
inline constexpr float kUiWidth = 640.0F;
inline constexpr float kUiHeight = 480.0F;

enum class UiFit : std::uint8_t { Centered, Stretched };

/// Where views that aren't on the UI canvas go: within the canvas's area, or the whole window.
enum class SceneFit : std::uint8_t { Canvas, Window };

struct PixelRect {
    std::uint32_t xPx = 0;
    std::uint32_t yPx = 0;
    std::uint32_t widthPx = 0;
    std::uint32_t heightPx = 0;

    bool operator==(const PixelRect&) const = default;
};

struct UiToClip {
    float scaleX = 0.0F;
    float scaleY = 0.0F;
    float offsetX = 0.0F;
    float offsetY = 0.0F;
};

/// `Centered` is the largest whole-pixel 4:3 area that fits.
[[nodiscard]] PixelRect FitUiArea(std::uint32_t widthPx, std::uint32_t heightPx, UiFit fit);

[[nodiscard]] Viewport UiViewport(const Rect& rect);

[[nodiscard]] PixelRect ViewportArea(const Viewport& viewport, std::uint32_t widthPx,
                                     std::uint32_t heightPx, UiFit uiFit, SceneFit sceneFit);

/// Under `SceneFit::Canvas` a window view keeps the canvas's shape, so a stretched UI stretches it.
[[nodiscard]] float ViewAspect(const Viewport& viewport, const PixelRect& area, SceneFit sceneFit);

/// UI units from the canvas's side to the window's when the scene fills a wider window, else 0.
[[nodiscard]] float EdgeShift(std::uint32_t widthPx, std::uint32_t heightPx, UiFit uiFit,
                              SceneFit sceneFit);

/// `edgeShift` cut so the sprites it moves stay within a centred area `hudAspect` wide.
[[nodiscard]] float HudShift(float edgeShift, std::optional<float> hudAspect);

[[nodiscard]] UiToClip UiToClipTransform(const PixelRect& area, std::uint32_t widthPx,
                                         std::uint32_t heightPx);

}
