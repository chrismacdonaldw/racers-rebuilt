#pragma once

#include <string_view>
#include <vector>

#include "app/ui_text/ui_font.hpp"
#include "render/render_view.hpp"

namespace rr::app {

struct TextPlacement {
    float x = 0.0F;
    float y = 0.0F;
    float scale = 1.0F;
    render::Color tint;
};

/// In UI units at scale 1.
[[nodiscard]] float TextWidth(const UiFont& font, std::u16string_view text);

/// `x` and `y` are the top left of the scaled text. Lower-case letters without a glyph use the
/// upper-case one, and other characters without a glyph are skipped.
void LayoutText(const UiFont& font, std::u16string_view text, const TextPlacement& placement,
                std::vector<render::SpriteDraw>& out);

/// Breaks as the original's fonts do, so a line also needs room for one more spacing.
[[nodiscard]] std::vector<std::u16string_view> WrapText(const UiFont& font,
                                                        std::u16string_view text, float width);

}
