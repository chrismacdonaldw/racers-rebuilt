#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "app/ui_text/ui_font.hpp"
#include "core/duration.hpp"
#include "render/render_view.hpp"

namespace rr::app {

inline constexpr std::string_view kBuildHelpFont = "font_hlp";

/// Shows the focused control's help once the keys rest, as the original does for the pointer.
class HelpTimer {
public:
    void Tick(Milliseconds elapsed, std::size_t target, bool pressed, bool hasHelp);

    [[nodiscard]] bool IsShown() const { return shown_ > Milliseconds{0}; }

private:
    std::size_t target_ = 0;
    Milliseconds resting_{0};
    Milliseconds shown_{0};
    Milliseconds quick_{0};
};

struct HelpBox {
    render::Rect border;
    render::Rect panel;
    float x = 0.0F;
    float y = 0.0F;
    std::vector<std::u16string_view> lines;
};

/// Wraps `text` and places it beside `target` inside `screen`, both in UI units.
[[nodiscard]] HelpBox LayoutHelp(const UiFont& font, std::u16string_view text,
                                 const render::Rect& target, const render::Rect& screen);

void DrawHelp(const UiFont& font, const HelpBox& box, std::vector<render::SpriteDraw>& out);

}
