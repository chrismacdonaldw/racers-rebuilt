#include "app/race_loading_screen.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rr::app {
namespace {

constexpr render::TextureId kBackground{3};
constexpr render::TextureId kTick{4};
constexpr render::TextureId kFont{5};
constexpr std::uint32_t kGlyphWidth = 10;

UiFont MonospaceFont() {
    UiFont font{.texture = kFont,
                .widthPx = kGlyphWidth * 64,
                .heightPx = 16,
                .glyphs = {.spaceWidthPx = kGlyphWidth}};
    for (char16_t character = u'!'; character <= u'Z'; ++character) {
        font.glyphs.glyphs.push_back({.character = character, .leftPx = 0, .widthPx = kGlyphWidth});
    }
    return font;
}

LoadingScreenParts Parts() {
    return {.background = kBackground,
            .tick = kTick,
            .ticks = {{0.12F, 0.9F}, {0.16F, 0.9F}, {0.2F, 0.9F}, {0.24F, 0.9F}},
            .trackName = u"ROCKET CITY RUN"};
}

std::vector<render::SpriteDraw> Layout(float progress) {
    const UiFont font = MonospaceFont();
    std::vector<render::SpriteDraw> out;
    LayoutLoadingScreen(Parts(), {.text = &font, .numbers = &font}, progress, out);
    return out;
}

std::vector<render::SpriteDraw> WithTexture(const std::vector<render::SpriteDraw>& sprites,
                                            render::TextureId texture) {
    std::vector<render::SpriteDraw> found;
    for (const render::SpriteDraw& sprite : sprites) {
        if (sprite.texture == texture) {
            found.push_back(sprite);
        }
    }
    return found;
}

}

TEST_CASE("the loading screen picture fills the screen under everything else") {
    const std::vector<render::SpriteDraw> out = Layout(0.0F);

    REQUIRE_FALSE(out.empty());
    CHECK(out[0].texture == kBackground);
    CHECK(out[0].bounds.x == 0.0F);
    CHECK(out[0].bounds.width == 640.0F);
    CHECK(out[0].bounds.height == 480.0F);
    CHECK(WithTexture(out, kTick).empty());
}

TEST_CASE("the track name is centred near the top") {
    const std::vector<render::SpriteDraw> glyphs = WithTexture(Layout(0.0F), kFont);

    REQUIRE(glyphs.size() == 13);
    CHECK(glyphs[0].bounds.x == doctest::Approx(245.0F));
    CHECK(glyphs[0].bounds.y == doctest::Approx(25.0F));
}

TEST_CASE("each tick is a 16 square centred on its spot, truncated like the original") {
    const std::vector<render::SpriteDraw> ticks = WithTexture(Layout(0.5F), kTick);

    REQUIRE(ticks.size() == 2);
    CHECK(ticks[0].bounds.x == 76.0F - 8.0F);
    CHECK(ticks[0].bounds.y == 431.0F - 8.0F);
    CHECK(ticks[0].bounds.width == 16.0F);
    CHECK(ticks[1].bounds.x == 102.0F - 8.0F);
}

TEST_CASE("ticks only appear once their share of the progress is complete") {
    CHECK(WithTexture(Layout(0.24F), kTick).empty());
    CHECK(WithTexture(Layout(0.25F), kTick).size() == 1);
    CHECK(WithTexture(Layout(1.0F), kTick).size() == 4);
    CHECK(WithTexture(Layout(2.0F), kTick).size() == 4);
}

}
