#include "app/cutscene/cutscene_overlay.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

using assets::EffectKind;

constexpr render::TextureId kPicture{3};
constexpr render::TextureId kFont{5};
constexpr std::uint32_t kGlyphWidth = 10;
constexpr render::Rect kFullScreen{.width = render::kUiWidth, .height = render::kUiHeight};

UiFont MonospaceFont() {
    UiFont font{.texture = kFont, .widthPx = kGlyphWidth * 64, .heightPx = 16};
    for (char16_t character = u'A'; character <= u'Z'; ++character) {
        font.glyphs.glyphs.push_back({.character = character, .leftPx = 0, .widthPx = kGlyphWidth});
    }
    return font;
}

std::vector<render::SpriteDraw> Drawn(const CutsceneOverlay& overlay,
                                      const render::Rect& viewport = kFullScreen) {
    std::vector<render::SpriteDraw> out;
    overlay.Draw(viewport, out);
    return out;
}

CutsceneOverlay PictureOverlay(const assets::CutsceneEffectsAsset& effects) {
    return {effects, {.images = {{.texture = kPicture, .widthPx = 100, .heightPx = 50}}}};
}

TEST_CASE("an image without edges centres in the cutscene's rectangle at its own size") {
    const assets::CutsceneEffectsAsset effects{.images = {{.name = "splash"}}};
    CutsceneOverlay overlay = PictureOverlay(effects);
    CHECK(Drawn(overlay).empty());

    overlay.Start(EffectKind::Image, 0);
    const std::vector<render::SpriteDraw> shown =
        Drawn(overlay, {.x = 5.0F, .y = 35.0F, .width = 630.0F, .height = 420.0F});

    REQUIRE(shown.size() == 1);
    CHECK(shown[0].texture == kPicture);
    CHECK(shown[0].bounds.x == 270.0F);
    CHECK(shown[0].bounds.y == 220.0F);
    CHECK(shown[0].bounds.width == 100.0F);
    CHECK(shown[0].bounds.height == 50.0F);

    overlay.Stop(EffectKind::Image, 0);
    CHECK(Drawn(overlay).empty());
}

TEST_CASE("edges and sizes are fractions of the target, and a visual past its edge isn't drawn") {
    assets::CutsceneEffectsAsset effects{
        .images = {{.layout = {.left = 0.5F, .top = 0.25F, .width = 0.25F, .height = 0.5F}}}};
    CutsceneOverlay sized = PictureOverlay(effects);
    sized.Start(EffectKind::Image, 0);
    const std::vector<render::SpriteDraw> shown = Drawn(sized);
    REQUIRE(shown.size() == 1);
    CHECK(shown[0].bounds.x == 320.0F);
    CHECK(shown[0].bounds.y == 120.0F);
    CHECK(shown[0].bounds.width == 160.0F);
    CHECK(shown[0].bounds.height == 240.0F);

    effects.images[0].layout = {.left = 0.9F};
    CutsceneOverlay right = PictureOverlay(effects);
    right.Start(EffectKind::Image, 0);
    CHECK(Drawn(right).empty());

    effects.images[0].layout = {.top = 0.95F};
    CutsceneOverlay bottom = PictureOverlay(effects);
    bottom.Start(EffectKind::Image, 0);
    CHECK(Drawn(bottom).empty());
}

TEST_CASE("text centres across the rectangle at its top, white unless it has a colour") {
    const UiFont font = MonospaceFont();
    const assets::CutsceneEffectsAsset effects{
        .texts = {{.layout = {.top = 0.75F}}, {.layout = {.top = 0.5F, .color = {{255, 0, 0}}}}}};
    CutsceneOverlay overlay(
        effects, {.texts = {{.text = u"AB", .font = &font}, {.text = u"C", .font = &font}}});
    overlay.Start(EffectKind::Text, 0);
    overlay.Start(EffectKind::Text, 1);

    const std::vector<render::SpriteDraw> shown = Drawn(overlay);

    REQUIRE(shown.size() == 3);
    CHECK(shown[0].bounds.x == 310.0F);
    CHECK(shown[0].bounds.y == 360.0F);
    CHECK(shown[0].tint.g == 1.0F);
    CHECK(shown[2].bounds.x == 315.0F);
    CHECK(shown[2].bounds.y == 240.0F);
    CHECK(shown[2].tint.r == 1.0F);
    CHECK(shown[2].tint.g == 0.0F);
}

TEST_CASE("images draw under text, and fades over both") {
    const UiFont font = MonospaceFont();
    const assets::CutsceneEffectsAsset effects{
        .fades = {{.duration = Milliseconds{100}, .direction = assets::FadeDirection::ToColor}},
        .texts = {{}},
        .images = {{}}};
    CutsceneOverlay overlay(effects,
                            {.texts = {{.text = u"A", .font = &font}},
                             .images = {{.texture = kPicture, .widthPx = 10, .heightPx = 10}}});
    overlay.Start(EffectKind::Fade, 0);
    overlay.Start(EffectKind::Text, 0);
    overlay.Start(EffectKind::Image, 0);

    const std::vector<render::SpriteDraw> shown = Drawn(overlay);

    REQUIRE(shown.size() == 3);
    CHECK(shown[0].texture == kPicture);
    CHECK(shown[1].texture == kFont);
    CHECK(shown[2].texture == render::kWhiteTexture);
}

float FadeAlpha(const CutsceneOverlay& overlay) {
    const std::vector<render::SpriteDraw> shown = Drawn(overlay);
    REQUIRE(shown.size() == 1);
    return shown.empty() ? -1.0F : shown[0].tint.a;
}

TEST_CASE("a fade ramps over its duration, holds its end for one update, then ends") {
    const assets::CutsceneEffectsAsset effects{
        .fades = {{.duration = Milliseconds{100},
                   .direction = assets::FadeDirection::FromColor,
                   .color = {255, 255, 255}},
                  {.duration = Milliseconds{100}, .direction = assets::FadeDirection::ToColor},
                  {.duration = Milliseconds{100}}}};
    CutsceneOverlay overlay(effects, {});

    overlay.Start(EffectKind::Fade, 0);
    CHECK(FadeAlpha(overlay) == 1.0F);
    overlay.Advance(Milliseconds{25});
    CHECK(FadeAlpha(overlay) == doctest::Approx(0.75F));
    overlay.Advance(Milliseconds{100});
    CHECK(FadeAlpha(overlay) == 0.0F);
    overlay.Advance(Milliseconds{10});
    CHECK(Drawn(overlay).empty());

    overlay.Start(EffectKind::Fade, 1);
    overlay.Advance(Milliseconds{25});
    CHECK(FadeAlpha(overlay) == doctest::Approx(0.25F));
    overlay.Stop(EffectKind::Fade, 1);
    CHECK(Drawn(overlay).empty());

    overlay.Start(EffectKind::Fade, 2);
    CHECK(Drawn(overlay).empty());
}

}
}
