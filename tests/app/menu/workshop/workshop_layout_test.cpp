#include "app/menu/workshop/workshop_layout.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

namespace rr::app {
namespace {

constexpr render::Rect kFourThree{.width = 640.0F, .height = 480.0F};
constexpr render::Rect kSixteenNine{.x = -106.667F, .width = 853.333F, .height = 480.0F};
constexpr render::Rect kThirtyTwoNine{.x = -533.333F, .width = 1706.667F, .height = 480.0F};

bool IsInside(const render::Rect& inner, const render::Rect& outer) {
    return inner.x >= outer.x && inner.y >= outer.y &&
           inner.x + inner.width <= outer.x + outer.width + 0.01F &&
           inner.y + inner.height <= outer.y + outer.height + 0.01F;
}

bool Overlaps(const render::Rect& a, const render::Rect& b) {
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height &&
           b.y < a.y + a.height;
}

bool ButtonsFit(const WorkshopLayout& layout) {
    return std::ranges::all_of(layout.tools, [&](const render::Rect& tool) {
        return IsInside(tool, layout.scene) && !Overlaps(tool, layout.done);
    });
}

bool DrawerFits(const WorkshopLayout& layout) {
    const render::Rect& tiles = layout.tiles.area;
    return IsInside(layout.tabs, layout.drawer) && IsInside(tiles, layout.drawer) &&
           IsInside(layout.counts, layout.drawer) && tiles.y > layout.tabs.y + layout.tabs.height &&
           tiles.y + tiles.height < layout.counts.y;
}

bool SplitsWindow(const render::Rect& window) {
    const WorkshopLayout layout = LayOutWorkshop(window, kTabColumns * kTabRows);
    const auto near = [](float a, float b) { return std::abs(a - b) < 0.01F; };
    return near(layout.drawer.x, window.x) && near(layout.drawer.height, window.height) &&
           near(layout.scene.x, layout.drawer.x + layout.drawer.width) &&
           near(layout.scene.x + layout.scene.width, window.x + window.width);
}

bool PanelsFit(const render::Rect& window) {
    const WorkshopLayout layout = LayOutWorkshop(window, kTabColumns * kTabRows);
    return DrawerFits(layout) && ButtonsFit(layout) && IsInside(layout.done, layout.scene) &&
           IsInside(layout.colours, layout.scene) && IsInside(layout.hint, layout.scene) &&
           layout.hint.y + layout.hint.height < layout.colours.y;
}

TEST_CASE("the drawer runs down the left edge and the scene fills the rest") {
    CHECK(SplitsWindow(kFourThree));
    CHECK(SplitsWindow(kSixteenNine));
    CHECK(SplitsWindow(kThirtyTwoNine));
}

TEST_CASE("every panel fits its part of the window at any aspect") {
    CHECK(PanelsFit(kFourThree));
    CHECK(PanelsFit(kSixteenNine));
    CHECK(PanelsFit(kThirtyTwoNine));
}

TEST_CASE("the drawer shows square tiles in as many rows as fit") {
    const WorkshopLayout layout = LayOutWorkshop(kFourThree, kTabColumns * kTabRows);
    const PaletteGrid& tiles = layout.tiles;
    const float side = (tiles.area.width - (tiles.gap * static_cast<float>(tiles.columns - 1))) /
                       static_cast<float>(tiles.columns);

    CHECK(tiles.columns == 3);
    CHECK(tiles.rows >= 3);
    CHECK(tiles.area.height ==
          doctest::Approx((static_cast<float>(tiles.rows) * (side + tiles.gap)) - tiles.gap));
    CHECK(LayOutWorkshop({.width = 640.0F, .height = 960.0F}, kTabColumns * kTabRows).tiles.rows >
          tiles.rows);
}

TEST_CASE("fewer tabs leave more room for tiles") {
    const WorkshopLayout full = LayOutWorkshop(kFourThree, kTabColumns * kTabRows);
    const WorkshopLayout one = LayOutWorkshop(kFourThree, kTabColumns);

    CHECK(one.tabs.height < full.tabs.height);
    CHECK(one.tiles.rows > full.tiles.rows);
    CHECK(LayOutWorkshop(kFourThree, 0).tabs.height == doctest::Approx(one.tabs.height));
}

TEST_CASE("tabs fill a row before starting the next") {
    const WorkshopLayout layout = LayOutWorkshop(kFourThree, kTabColumns * kTabRows);

    CHECK(TabOf(layout, 1).x > TabOf(layout, 0).x);
    CHECK(TabOf(layout, 1).y == doctest::Approx(TabOf(layout, 0).y));
    CHECK(TabOf(layout, kTabColumns).x == doctest::Approx(TabOf(layout, 0).x));
    CHECK(TabOf(layout, kTabColumns).y > TabOf(layout, 0).y);
    CHECK(IsInside(TabOf(layout, (kTabColumns * kTabRows) - 1), layout.tabs));
}

}
}
