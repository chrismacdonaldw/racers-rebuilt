#include "app/menu/racer_lock.hpp"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>

#include "app/menu/build_help.hpp"
#include "app/racer/racer_models.hpp"
#include "app/ui_text/text_layout.hpp"
#include "assets/driver_model.hpp"
#include "assets/ldraw_extra_pieces.hpp"
#include "assets/piece_catalog.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::u16string_view kExtraPiecesNotice = u"USES EXTRA PIECES";
constexpr std::u16string_view kMissingPiecesNotice = u"NEEDS PIECES FROM A MOD";
constexpr std::u16string_view kMissingPartsNotice = u"NEEDS PARTS FROM A MOD";
constexpr float kMargin = 8.0F;
constexpr render::Color kNoticeColor{.r = 1.0F, .g = 0.35F, .b = 0.25F};

std::u16string_view NoticeOf(RacerLock lock) {
    switch (lock) {
        case RacerLock::ExtraPieces:
            return kExtraPiecesNotice;
        case RacerLock::MissingPieces:
            return kMissingPiecesNotice;
        case RacerLock::None:
        case RacerLock::MissingParts:
            break;
    }
    return kMissingPartsNotice;
}

}

bool IsLockedCar(const assets::CarData& car, bool allowsExtraPieces) {
    return !allowsExtraPieces &&
           std::ranges::any_of(assets::PieceTypesOf(car), assets::IsExtraPieceType);
}

RacerLocks::RacerLocks(std::optional<assets::DriverPartCatalog> parts,
                       std::optional<std::vector<std::uint16_t>> pieceTypes)
    : parts_(std::move(parts)), pieceTypes_(std::move(pieceTypes)) {}

RacerLocks RacerLocks::Load(const assets::Vfs& vfs) {
    Result<assets::DriverPartCatalog> parts = LoadDriverParts(vfs);
    if (!parts) {
        log::Warn("{}, no driver locks for missing parts", parts.error().message);
    }
    Result<std::vector<std::uint16_t>> types = assets::LoadPieceTypes(vfs);
    if (!types) {
        log::Warn("{}, no car locks for missing pieces", types.error().message);
    }
    return RacerLocks(parts ? std::optional(std::move(*parts)) : std::nullopt,
                      types ? std::optional(std::move(*types)) : std::nullopt);
}

RacerLock RacerLocks::Of(const settings::Racer& racer) const {
    if (IsLockedCar(racer.car, allowsExtraPieces_)) {
        return RacerLock::ExtraPieces;
    }
    if (pieceTypes_ &&
        std::ranges::any_of(assets::PieceTypesOf(racer.car), [&](std::uint16_t type) {
            return !std::ranges::contains(*pieceTypes_, type);
        })) {
        return RacerLock::MissingPieces;
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
    const std::u16string_view notice = NoticeOf(lock);
    const float width = TextWidth(*font, notice);
    LayoutText(*font, notice,
               {.x = area.x + ((area.width - width) / 2.0F),
                .y = area.y + area.height - static_cast<float>(font->heightPx) - kMargin,
                .tint = kNoticeColor},
               out);
}

}
