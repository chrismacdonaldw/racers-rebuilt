#include "app/menu/icon_strip.hpp"

#include <doctest/doctest.h>

#include <vector>

namespace rr::app {
namespace {

constexpr Milliseconds kSlide{400};
constexpr std::size_t kItems = 5;

StripPage PageOf(const IconStrip& strip, std::size_t page) {
    return strip.Pages().at(page).value_or(StripPage{.item = 99});
}

TEST_CASE("a strip at rest shows one page") {
    IconStrip strip(kSlide);
    strip.Snap(3, kItems);

    CHECK(PageOf(strip, 0).item == 3);
    CHECK(PageOf(strip, 0).shift == doctest::Approx(0.0F));
    CHECK_FALSE(strip.Pages().back().has_value());
}

TEST_CASE("a slide moves a page in steady steps and ignores steps meanwhile") {
    IconStrip strip(kSlide);
    strip.Snap(1, kItems);
    REQUIRE(strip.Slide(1, kItems));
    strip.Tick(Milliseconds{100});

    CHECK(PageOf(strip, 0).item == 1);
    CHECK(PageOf(strip, 0).shift == doctest::Approx(-0.25F));
    CHECK(PageOf(strip, 1).item == 2);
    CHECK(PageOf(strip, 1).shift == doctest::Approx(0.75F));
    CHECK_FALSE(strip.Slide(1, kItems));

    strip.Tick(Milliseconds{300});
    CHECK_FALSE(strip.IsSliding());
    CHECK(PageOf(strip, 0).item == 2);
}

TEST_CASE("sliding back past the first page wraps to the last") {
    IconStrip strip(kSlide);
    strip.Snap(0, kItems);
    REQUIRE(strip.Slide(-1, kItems));
    strip.Tick(Milliseconds{100});

    CHECK(PageOf(strip, 0).item == 4);
    CHECK(PageOf(strip, 0).shift == doctest::Approx(-0.75F));
    CHECK(PageOf(strip, 1).item == 0);

    strip.Tick(kSlide);
    CHECK(PageOf(strip, 0).item == 4);
    CHECK(PageOf(strip, 0).shift == doctest::Approx(0.0F));
}

TEST_CASE("a strip icon is cropped to the box as it slides out") {
    const UiImage icon{.widthPx = 40, .heightPx = 20};
    const render::Rect box{.x = 100.0F, .y = 50.0F, .width = 80.0F, .height = 40.0F};
    std::vector<render::SpriteDraw> sprites;

    DrawStripIcon(icon, box, -0.5F, sprites);
    DrawStripIcon(icon, box, -1.0F, sprites);

    REQUIRE(sprites.size() == 1);
    CHECK(sprites.front().bounds.x == doctest::Approx(100.0F));
    CHECK(sprites.front().bounds.width == doctest::Approx(20.0F));
    CHECK(sprites.front().uv.x == doctest::Approx(0.5F));
    CHECK(sprites.front().uv.width == doctest::Approx(0.5F));
}

}
}
