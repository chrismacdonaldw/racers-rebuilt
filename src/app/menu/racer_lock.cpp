#include "app/menu/racer_lock.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

#include "app/menu/build_help.hpp"
#include "app/racer/racer_models.hpp"
#include "app/ui_text/text_layout.hpp"
#include "assets/driver_model.hpp"
#include "assets/ldraw_extra_pieces.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::u16string_view kExtraPiecesNotice = u"Uses extra pieces";
constexpr std::u16string_view kMissingPartsNotice = u"Needs parts from a mod";
constexpr float kMargin = 8.0F;
constexpr render::Color kNoticeColor{.r = 1.0F, .g = 0.35F, .b = 0.25F};

}

bool IsLockedCar(const assets::CarData& car, bool allowsExtraPieces) {
    return !allowsExtraPieces &&
           std::ranges::any_of(assets::PieceTypesOf(car), assets::IsExtraPieceType);
}

RacerLocks::RacerLocks(std::optional<assets::DriverPartCatalog> parts) : parts_(std::move(parts)) {}

RacerLocks RacerLocks::Load(const assets::Vfs& vfs) {
    Result<assets::DriverPartCatalog> parts = LoadDriverParts(vfs);
    if (!parts) {
        log::Warn("{}, no driver locks for missing parts", parts.error().message);
        return RacerLocks(std::nullopt);
    }
    return RacerLocks(std::move(*parts));
}

RacerLock RacerLocks::Of(const settings::Racer& racer) const {
    if (IsLockedCar(racer.car, allowsExtraPieces_)) {
        return RacerLock::ExtraPieces;
    }
    if (parts_ && assets::HasMissingParts(*parts_, racer.look)) {
        return RacerLock::MissingParts;
    }
    return RacerLock::None;
}

void DrawLockNotice(const MenuText& text, RacerLock lock, const render::Rect& area,
                    std::vector<render::SpriteDraw>& out) {
    const UiFont* font = text.Font(kBuildHelpFont);
    if (font == nullptr || lock == RacerLock::None) {
        return;
    }
    const std::u16string_view notice =
        lock == RacerLock::ExtraPieces ? kExtraPiecesNotice : kMissingPartsNotice;
    const float width = TextWidth(*font, notice);
    LayoutText(*font, notice,
               {.x = area.x + ((area.width - width) / 2.0F),
                .y = area.y + area.height - static_cast<float>(font->heightPx) - kMargin,
                .tint = kNoticeColor},
               out);
}

}
