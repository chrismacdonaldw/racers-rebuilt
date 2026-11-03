#include "render/ui_layout.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>

namespace rr::render {
namespace {

struct Window {
    std::uint32_t widthPx = 0;
    std::uint32_t heightPx = 0;
    PixelRect centered;
};

constexpr std::array kWindows{
    Window{.widthPx = 1280,
           .heightPx = 720,
           .centered = {.xPx = 160, .yPx = 0, .widthPx = 960, .heightPx = 720}},
    Window{.widthPx = 1920,
           .heightPx = 1080,
           .centered = {.xPx = 240, .yPx = 0, .widthPx = 1440, .heightPx = 1080}},
    Window{.widthPx = 3440,
           .heightPx = 1440,
           .centered = {.xPx = 760, .yPx = 0, .widthPx = 1920, .heightPx = 1440}},
    Window{.widthPx = 1024,
           .heightPx = 1024,
           .centered = {.xPx = 0, .yPx = 128, .widthPx = 1024, .heightPx = 768}},
};

struct ClipPoint {
    float x = 0.0F;
    float y = 0.0F;
};

ClipPoint ToClip(const UiToClip& transform, float x, float y) {
    return {.x = (x * transform.scaleX) + transform.offsetX,
            .y = (y * transform.scaleY) + transform.offsetY};
}

TEST_CASE("centered UI keeps 4:3 at the largest whole-pixel size") {
    for (const Window& window : kWindows) {
        CAPTURE(window.widthPx);
        CAPTURE(window.heightPx);
        CHECK(FitUiArea(window.widthPx, window.heightPx, UiFit::Centered) == window.centered);
    }
}

TEST_CASE("stretched UI fills the window") {
    for (const Window& window : kWindows) {
        CHECK(FitUiArea(window.widthPx, window.heightPx, UiFit::Stretched) ==
              PixelRect{.widthPx = window.widthPx, .heightPx = window.heightPx});
    }
}

TEST_CASE("the UI canvas corners land on the area's corners in clip space") {
    const Window& window = kWindows[0];
    const UiToClip transform = UiToClipTransform(window.centered, window.widthPx, window.heightPx);

    const ClipPoint topLeft = ToClip(transform, 0.0F, 0.0F);
    const ClipPoint bottomRight = ToClip(transform, kUiWidth, kUiHeight);

    CHECK(topLeft.x == doctest::Approx(-0.75F));
    CHECK(topLeft.y == doctest::Approx(1.0F));
    CHECK(bottomRight.x == doctest::Approx(0.75F));
    CHECK(bottomRight.y == doctest::Approx(-1.0F));
}

constexpr Viewport kWholeWindow{};
constexpr Viewport kLowerHalf{.y = 0.5F, .height = 0.5F};
constexpr std::uint32_t kWideWidthPx = 1920;
constexpr std::uint32_t kWideHeightPx = 1080;

PixelRect WideArea(const Viewport& viewport, UiFit uiFit, SceneFit sceneFit) {
    return ViewportArea(viewport, kWideWidthPx, kWideHeightPx, uiFit, sceneFit);
}

TEST_CASE("a window view keeps to the 4:3 area unless the scene fits the window") {
    CHECK(WideArea(kWholeWindow, UiFit::Centered, SceneFit::Canvas) ==
          PixelRect{.xPx = 240, .widthPx = 1440, .heightPx = 1080});
    CHECK(WideArea(kLowerHalf, UiFit::Centered, SceneFit::Canvas) ==
          PixelRect{.xPx = 240, .yPx = 540, .widthPx = 1440, .heightPx = 540});
    CHECK(WideArea(kWholeWindow, UiFit::Centered, SceneFit::Window) ==
          PixelRect{.widthPx = 1920, .heightPx = 1080});
    CHECK(WideArea(kLowerHalf, UiFit::Centered, SceneFit::Window) ==
          PixelRect{.yPx = 540, .widthPx = 1920, .heightPx = 540});
}

TEST_CASE("a view reaching past the window's left edge clips to it") {
    const Viewport pastLeft{.x = -0.5F, .width = 1.0F};

    CHECK(WideArea(pastLeft, UiFit::Centered, SceneFit::Window) ==
          PixelRect{.widthPx = 960, .heightPx = 1080});
}

TEST_CASE(
    "anchors shift by the side bar's width in UI units only when the scene fills the window") {
    CHECK(EdgeShift(kWideWidthPx, kWideHeightPx, UiFit::Centered, SceneFit::Window) ==
          doctest::Approx(240.0F * 640.0F / 1440.0F));
    CHECK(EdgeShift(kWideWidthPx, kWideHeightPx, UiFit::Centered, SceneFit::Canvas) == 0.0F);
    CHECK(EdgeShift(kWideWidthPx, kWideHeightPx, UiFit::Stretched, SceneFit::Window) == 0.0F);
    CHECK(EdgeShift(1024, 1024, UiFit::Centered, SceneFit::Window) == 0.0F);
    CHECK(EdgeShift(0, 0, UiFit::Centered, SceneFit::Window) == 0.0F);
}

TEST_CASE("a canvas view stays on the canvas whatever the scene fit") {
    const Viewport onCanvas = UiViewport({.x = 320.0F, .width = 320.0F, .height = 480.0F});

    CHECK(WideArea(onCanvas, UiFit::Centered, SceneFit::Window) ==
          PixelRect{.xPx = 960, .widthPx = 720, .heightPx = 1080});
}

TEST_CASE("a 4:3 window view keeps the canvas's shape, even when stretched") {
    const PixelRect stretched = WideArea(kWholeWindow, UiFit::Stretched, SceneFit::Canvas);
    const PixelRect wide = WideArea(kLowerHalf, UiFit::Centered, SceneFit::Window);

    CHECK(ViewAspect(kWholeWindow, stretched, SceneFit::Canvas) == doctest::Approx(4.0F / 3.0F));
    CHECK(ViewAspect(kLowerHalf, stretched, SceneFit::Canvas) == doctest::Approx(8.0F / 3.0F));
    CHECK(ViewAspect(kLowerHalf, wide, SceneFit::Window) == doctest::Approx(1920.0F / 540.0F));
}

}
}
