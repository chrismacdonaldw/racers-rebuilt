#include "app/hud/hud_text.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace rr::app {
namespace {

UiFont OneGlyphFont(render::TextureId texture, std::uint32_t width, std::uint32_t height) {
    return {.texture = texture,
            .widthPx = width,
            .heightPx = height,
            .glyphs = {.glyphs = {{.character = u'1', .leftPx = 0, .widthPx = width}}}};
}

TEST_CASE("hud text draws each text with its own font") {
    const UiFont text = OneGlyphFont(render::TextureId{1}, 4, 8);
    const UiFont numbers = OneGlyphFont(render::TextureId{2}, 10, 20);
    const HudFonts fonts{.text = &text, .numbers = &numbers};
    const std::array texts{
        HudText{.text = u"11", .x = 3.0F, .y = 5.0F},
        HudText{.font = HudFont::Numbers, .text = u"1", .x = 50.0F, .y = 6.0F, .scale = 2.0F}};
    std::vector<render::SpriteDraw> sprites;

    DrawHudText(texts, fonts, sprites);

    REQUIRE(sprites.size() == 3);
    CHECK(sprites[0].texture == render::TextureId{1});
    CHECK(sprites[1].bounds.x == 7.0F);
    CHECK(sprites[2].texture == render::TextureId{2});
    CHECK(sprites[2].bounds.x == 50.0F);
    CHECK(sprites[2].bounds.height == 40.0F);
    CHECK(fonts.Width(HudFont::Numbers, u"11") == 20.0F);
    CHECK(fonts.Height(HudFont::Text) == 8.0F);
}

}
}
