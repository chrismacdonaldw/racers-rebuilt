#include "app/ui_text/text_layout.hpp"

#include <doctest/doctest.h>

#include <array>
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

UiFont ShapesFont() {
    UiFont font = TwoLetterFont();
    font.fixes.missingGlyphs = MissingGlyphs::Shapes;
    return font;
}

TEST_CASE("text lays out glyphs left to right with spacing between them") {
    const UiFont font = TwoLetterFont();
    std::vector<render::SpriteDraw> sprites;

    LayoutText(font, u"Ab A", {.x = 5.0F, .y = 7.0F, .scale = 2.0F, .tint = {.r = 0.5F}}, sprites);

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

TEST_CASE("text width leaves out trailing spacing and line breaks") {
    const UiFont font = TwoLetterFont();

    CHECK(TextWidth(font, u"Ab A") == 20.0F);
    CHECK(TextWidth(font, u"") == 0.0F);
    CHECK(TextWidth(font, u"A\nA") == 9.0F);
}

TEST_CASE("a missing character draws the font's space cell, as the original does") {
    const UiFont font = TwoLetterFont();
    std::vector<render::SpriteDraw> sprites;

    LayoutText(font, u"B/", {}, sprites);

    REQUIRE(sprites.size() == 2);
    CHECK(sprites[1].texture == render::TextureId{3});
    CHECK(sprites[1].bounds.x == 7.0F);
    CHECK(sprites[1].bounds.width == 3.0F);
    CHECK(sprites[1].uv.x == 0.0F);
    CHECK(TextWidth(font, u"?") == 3.0F);
}

TEST_CASE("a glyph coded below the space comes first, so a missing character draws it") {
    UiFont font = TwoLetterFont();
    font.glyphs.glyphs.insert(font.glyphs.glyphs.begin(),
                              {.character = 5, .leftPx = 30, .widthPx = 7});
    std::vector<render::SpriteDraw> sprites;

    LayoutText(font, u"/", {}, sprites);

    REQUIRE(sprites.size() == 1);
    CHECK(sprites[0].bounds.width == 7.0F);
    CHECK(sprites[0].uv.x == doctest::Approx(0.3F));
}

TEST_CASE("with shapes, other missing characters are skipped") {
    const UiFont font = ShapesFont();

    CHECK(TextWidth(font, u"Ab A?") == 20.0F);
    CHECK(TextWidth(font, u"?") == 0.0F);
}

TEST_CASE("a missing dash or slash is drawn as a shape in the text's tint") {
    const UiFont font = ShapesFont();
    std::vector<render::SpriteDraw> sprites;

    LayoutText(font, u"A-B/", {.x = 5.0F, .scale = 2.0F, .tint = {.g = 0.5F}}, sprites);

    REQUIRE(sprites.size() == 4);
    CHECK(sprites[1].texture == render::kWhiteTexture);
    CHECK(sprites[1].bounds.x == 15.0F);
    CHECK(sprites[1].bounds.width == 8.0F);
    CHECK(sprites[1].tint.g == 0.5F);
    CHECK(sprites[2].bounds.x == 25.0F);
    REQUIRE(sprites[3].corners.has_value());
    CHECK(sprites[3].corners.value_or(std::array<render::SpriteCorner, 4>{})[0].tint.g == 0.5F);
    CHECK(TextWidth(font, u"A-B/") == 21.0F);
}

TEST_CASE("a font's own dash wins over the drawn one") {
    UiFont font = ShapesFont();
    font.glyphs.glyphs.insert(font.glyphs.glyphs.begin(),
                              {.character = u'-', .leftPx = 2, .widthPx = 2});
    std::vector<render::SpriteDraw> sprites;

    LayoutText(font, u"-", {}, sprites);

    REQUIRE(sprites.size() == 1);
    CHECK(sprites[0].texture == render::TextureId{3});
    CHECK(sprites[0].bounds.width == 2.0F);
}

TEST_CASE("fixed glyph lists are used only when the fix is on") {
    UiFont font = TwoLetterFont();
    font.fixedGlyphs = assets::FontGlyphs{
        .spaceWidthPx = 3, .glyphs = {{.character = u'A', .leftPx = 50, .widthPx = 5}}};

    CHECK(TextWidth(font, u"A") == 4.0F);
    font.fixes.fixesGlyphLists = true;
    CHECK(TextWidth(font, u"A") == 5.0F);
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
