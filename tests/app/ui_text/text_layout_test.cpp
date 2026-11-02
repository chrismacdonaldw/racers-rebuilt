#include "app/ui_text/text_layout.hpp"

#include <doctest/doctest.h>

#include <string_view>
#include <vector>

namespace rr::app {
namespace {

UiFont TwoLetterFont() {
    return {.texture = render::TextureId{3},
            .widthPx = 100,
            .heightPx = 8,
            .spacingPx = 1,
            .glyphs = {.spaceWidthPx = 3,
                       .glyphs = {{.character = u'A', .leftPx = 10, .widthPx = 4},
                                  {.character = u'B', .leftPx = 20, .widthPx = 6}}}};
}

TEST_CASE("text lays out glyphs left to right with spacing between them") {
    const UiFont font = TwoLetterFont();
    std::vector<render::SpriteDraw> sprites;

    LayoutText(font, u"Ab A?", {.x = 5.0F, .y = 7.0F, .scale = 2.0F, .tint = {.r = 0.5F}}, sprites);

    REQUIRE(sprites.size() == 3);
    CHECK(sprites[0].texture == render::TextureId{3});
    CHECK(sprites[0].bounds.x == 5.0F);
    CHECK(sprites[0].bounds.y == 7.0F);
    CHECK(sprites[0].bounds.width == 8.0F);
    CHECK(sprites[0].bounds.height == 16.0F);
    CHECK(sprites[0].uv.x == doctest::Approx(0.1F));
    CHECK(sprites[0].uv.width == doctest::Approx(0.04F));
    CHECK(sprites[0].tint.r == 0.5F);
    CHECK(sprites[1].bounds.x == 15.0F);
    CHECK(sprites[1].uv.x == doctest::Approx(0.2F));
    CHECK(sprites[2].bounds.x == 37.0F);
}

TEST_CASE("text width leaves out trailing spacing and missing glyphs") {
    const UiFont font = TwoLetterFont();

    CHECK(TextWidth(font, u"Ab A?") == 20.0F);
    CHECK(TextWidth(font, u"") == 0.0F);
    CHECK(TextWidth(font, u"?") == 0.0F);
}

using Lines = std::vector<std::u16string_view>;

TEST_CASE("text wraps at the last space that fits and drops the spaces it breaks at") {
    const UiFont font = TwoLetterFont();

    CHECK(WrapText(font, u"AB AB  BA", 15.0F) == Lines{u"AB", u"AB", u"BA"});
    CHECK(WrapText(font, u"AB AB", 28.0F) == Lines{u"AB AB"});
    CHECK(WrapText(font, u"AB AB", 27.0F) == Lines{u"AB", u"AB"});
    CHECK(WrapText(font, u"", 15.0F).empty());
    CHECK(WrapText(font, u"AB AB  ", 15.0F) == Lines{u"AB", u"AB", u""});
}

TEST_CASE("text wraps at line breaks and keeps the spaces after one") {
    const UiFont font = TwoLetterFont();

    CHECK(WrapText(font, u"A\r\n B\nA", 100.0F) == Lines{u"A", u" B", u"A"});
}

TEST_CASE("a word too wide for a line is cut where it stops fitting") {
    const UiFont font = TwoLetterFont();

    CHECK(WrapText(font, u"BBBA", 15.0F) == Lines{u"BB", u"BA"});
    CHECK(WrapText(font, u"B", 2.0F) == Lines{u"B"});
    CHECK(WrapText(font, u"AB", -1.0F) == Lines{u"A", u"B"});
}

}
}
