#include "app/menu/extra_pieces_lock.hpp"

#include <algorithm>
#include <string_view>

#include "app/menu/build_help.hpp"
#include "app/ui_text/text_layout.hpp"
#include "assets/ldraw_extra_pieces.hpp"

namespace rr::app {
namespace {

constexpr std::u16string_view kNotice = u"Uses extra pieces";
constexpr float kMargin = 8.0F;
constexpr render::Color kNoticeColor{.r = 1.0F, .g = 0.35F, .b = 0.25F};

}

bool IsLockedCar(const assets::CarData& car, bool allowsExtraPieces) {
    return !allowsExtraPieces &&
           std::ranges::any_of(assets::PieceTypesOf(car), assets::IsExtraPieceType);
}

void DrawLockNotice(const MenuText& text, const render::Rect& area,
                    std::vector<render::SpriteDraw>& out) {
    const UiFont* font = text.Font(kBuildHelpFont);
    if (font == nullptr) {
        return;
    }
    const float width = TextWidth(*font, kNotice);
    LayoutText(*font, kNotice,
               {.x = area.x + ((area.width - width) / 2.0F),
                .y = area.y + area.height - static_cast<float>(font->heightPx) - kMargin,
                .tint = kNoticeColor},
               out);
}

}
