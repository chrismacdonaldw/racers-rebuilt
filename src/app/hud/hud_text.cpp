#include "app/hud/hud_text.hpp"

#include <cstddef>

#include "app/ui_text/text_layout.hpp"

namespace rr::app {

const UiFont& HudFonts::Of(HudFont font) const {
    return font == HudFont::Numbers ? *numbers : *text;
}

float HudFonts::Width(HudFont font, std::u16string_view line) const {
    return TextWidth(Of(font), line);
}

float HudFonts::Height(HudFont font) const {
    return static_cast<float>(Of(font).heightPx);
}

void DrawHudText(std::span<const HudText> texts, const HudFonts& fonts,
                 std::vector<render::SpriteDraw>& out) {
    for (const HudText& text : texts) {
        const std::size_t first = out.size();
        LayoutText(fonts.Of(text.font), text.text,
                   {.x = text.x, .y = text.y, .scale = text.scale, .tint = text.tint}, out);
        AnchorSprites(std::span(out).subspan(first), text.anchor);
    }
}

void AnchorSprites(std::span<render::SpriteDraw> sprites, render::SpriteAnchor anchor) {
    for (render::SpriteDraw& sprite : sprites) {
        sprite.anchor = anchor;
    }
}

}
