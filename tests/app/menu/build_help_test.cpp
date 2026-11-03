#include "app/menu/build_help.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <string>

#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr std::uint32_t kGlyphWidth = 10;
constexpr render::Rect kScreen{.width = render::kUiWidth, .height = render::kUiHeight};

UiFont MonospaceFont() {
    UiFont font{.widthPx = kGlyphWidth * 64, .heightPx = 16};
    font.glyphs.spaceWidthPx = kGlyphWidth;
    for (char16_t character = u'A'; character <= u'Z'; ++character) {
        font.glyphs.glyphs.push_back({.character = character, .leftPx = 0, .widthPx = kGlyphWidth});
    }
    return font;
}

std::u16string TwentyWords() {
    std::u16string text = u"ABCD";
    for (int word = 1; word < 20; ++word) {
        text += u" ABCD";
    }
    return text;
}

HelpTimer ShownOn(std::size_t target) {
    HelpTimer timer;
    timer.Tick(Milliseconds{0}, target, false, true);
    timer.Tick(Milliseconds{3000}, target, false, true);
    return timer;
}

}

TEST_CASE("help shows once the keys rest for three seconds") {
    HelpTimer timer;
    timer.Tick(Milliseconds{2000}, 0, false, true);
    timer.Tick(Milliseconds{988}, 0, true, true);
    timer.Tick(Milliseconds{2000}, 0, false, true);
    CHECK_FALSE(timer.IsShown());

    timer.Tick(Milliseconds{12}, 0, false, true);
    CHECK(timer.IsShown());
    timer.Tick(Milliseconds{12}, 0, true, true);
    CHECK(timer.IsShown());
}

TEST_CASE("help goes after twenty seconds and stays gone until the focus moves") {
    HelpTimer timer = ShownOn(0);
    timer.Tick(Milliseconds{20000}, 0, false, true);
    CHECK_FALSE(timer.IsShown());
    timer.Tick(Milliseconds{5000}, 0, true, true);
    CHECK_FALSE(timer.IsShown());

    timer.Tick(Milliseconds{3000}, 1, false, true);
    CHECK(timer.IsShown());
}

TEST_CASE("moving on from shown help shows the next control's at once") {
    HelpTimer timer = ShownOn(0);
    timer.Tick(Milliseconds{12}, 1, true, true);
    CHECK(timer.IsShown());

    timer.Tick(Milliseconds{12}, 2, true, false);
    CHECK_FALSE(timer.IsShown());
    timer.Tick(Milliseconds{12}, 3, true, true);
    CHECK_FALSE(timer.IsShown());
}

TEST_CASE("help wraps to a squarish box beside its control") {
    const UiFont font = MonospaceFont();

    const HelpBox box =
        LayoutHelp(font, TwentyWords(), {.x = 20, .y = 200, .width = 40, .height = 40}, kScreen);

    CHECK(box.lines.size() == 5);
    CHECK(box.x == 76.0F);
    CHECK(box.y == 180.0F);
    CHECK(box.panel.width == 206.0F);
    CHECK(box.panel.height == 96.0F);
    CHECK(box.border.x == box.panel.x - 4.0F);
}

TEST_CASE("help without room beside its control goes below, kept on screen") {
    const UiFont font = MonospaceFont();

    const HelpBox box =
        LayoutHelp(font, TwentyWords(), {.x = 600, .y = 200, .width = 40, .height = 40}, kScreen);

    CHECK(box.x == 434.0F);
    CHECK(box.y == 256.0F);
}

}
