#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "app/ui_text/ui_font.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// `Text` is the original's `font_ths` and `Numbers` its `ignum`.
enum class HudFont : std::uint8_t { Text, Numbers };

/// Both fonts must outlive whatever holds them.
struct HudFonts {
    const UiFont* text = nullptr;
    const UiFont* numbers = nullptr;

    [[nodiscard]] const UiFont& Of(HudFont font) const;
    [[nodiscard]] float Width(HudFont font, std::u16string_view line) const;
    [[nodiscard]] float Height(HudFont font) const;
};

/// `x` and `y` are the top left of the text in UI units. `text` is valid until the next layout.
struct HudText {
    HudFont font = HudFont::Text;
    std::u16string_view text;
    float x = 0.0F;
    float y = 0.0F;
    float scale = 1.0F;
    render::Color tint;
};

/// Appends the glyph sprites of each text, in order.
void DrawHudText(std::span<const HudText> texts, const HudFonts& fonts,
                 std::vector<render::SpriteDraw>& out);

}
