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

}
}
