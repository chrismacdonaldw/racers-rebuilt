#include "app/menu_pointer.hpp"

#include <doctest/doctest.h>

namespace rr::app {
namespace {

TEST_CASE("window pixels map onto the canvas through the UI area") {
    const render::PixelRect area{.xPx = 160, .yPx = 0, .widthPx = 1280, .heightPx = 960};

    const CanvasPoint corner = ToCanvas(160.0F, 0.0F, area);
    const CanvasPoint middle = ToCanvas(800.0F, 480.0F, area);

    CHECK(corner.x == doctest::Approx(0.0F));
    CHECK(corner.y == doctest::Approx(0.0F));
    CHECK(middle.x == doctest::Approx(320.0F));
    CHECK(middle.y == doctest::Approx(240.0F));
}

TEST_CASE("the bars beside a centred canvas are off it") {
    const render::PixelRect area{.xPx = 160, .yPx = 0, .widthPx = 1280, .heightPx = 960};

    CHECK_FALSE(IsOnCanvas(ToCanvas(100.0F, 300.0F, area)));
    CHECK_FALSE(IsOnCanvas(ToCanvas(1500.0F, 300.0F, area)));
    CHECK(IsOnCanvas(ToCanvas(170.0F, 300.0F, area)));
    CHECK_FALSE(IsOnCanvas(ToCanvas(10.0F, 10.0F, {})));
}

}
}
