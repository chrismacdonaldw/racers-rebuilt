#pragma once

#include <cstdint>
#include <string_view>

#include "render/render_view.hpp"

namespace rr::app {

/// `Text` is the original's `font_ths` and `Numbers` its `ignum`.
enum class HudFont : std::uint8_t { Text, Numbers };

/// Text sizes in UI units at scale 1.
class HudMetrics {
public:
    virtual ~HudMetrics() = default;

    [[nodiscard]] virtual float Width(HudFont font, std::u16string_view text) const = 0;
    [[nodiscard]] virtual float Height(HudFont font) const = 0;
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

}
