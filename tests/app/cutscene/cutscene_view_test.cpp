#include "app/cutscene/cutscene_view.hpp"

#include <doctest/doctest.h>

#include <optional>

#include "render/ui_layout.hpp"

namespace rr::app {

TEST_CASE("full-screen shots fill the window unless kept to the canvas") {
    constexpr render::Rect kCanvas{.width = render::kUiWidth, .height = render::kUiHeight};

    CHECK_FALSE(CutsceneViewport(std::nullopt, false).isOnUiCanvas);
    CHECK_FALSE(CutsceneViewport(kCanvas, false).isOnUiCanvas);
    CHECK(CutsceneViewport(std::nullopt, true).isOnUiCanvas);
    CHECK(CutsceneViewport(kCanvas, true).width == 1.0F);
}

TEST_CASE("windowed shots stay on the canvas") {
    const render::Viewport windowed =
        CutsceneViewport(render::Rect{.x = 320.0F, .width = 320.0F, .height = 240.0F}, false);

    CHECK(windowed.isOnUiCanvas);
    CHECK(windowed.x == 0.5F);
    CHECK(windowed.height == 0.5F);
}

}
